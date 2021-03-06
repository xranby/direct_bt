/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <cstdio>

#include <algorithm>

// #define PERF_PRINT_ON 1
#include <dbt_debug.hpp>

#include "BTIoctl.hpp"

#include "DBTManager.hpp"
#include "HCIIoctl.hpp"
#include "HCIComm.hpp"
#include "DBTTypes.hpp"

extern "C" {
    #include <inttypes.h>
    #include <unistd.h>
    #include <poll.h>
    #include <signal.h>
}

using namespace direct_bt;

MgmtEnv::MgmtEnv()
: DEBUG_GLOBAL( DBTEnv::get().DEBUG ),
  exploding( DBTEnv::getExplodingProperties("direct_bt.mgmt") ),
  MGMT_READER_THREAD_POLL_TIMEOUT( DBTEnv::getInt32Property("direct_bt.mgmt.reader.timeout", 10000, 1500 /* min */, INT32_MAX /* max */) ),
  MGMT_COMMAND_REPLY_TIMEOUT( DBTEnv::getInt32Property("direct_bt.mgmt.cmd.timeout", 3000, 1500 /* min */, INT32_MAX /* max */) ),
  MGMT_EVT_RING_CAPACITY( DBTEnv::getInt32Property("direct_bt.mgmt.ringsize", 64, 64 /* min */, 1024 /* max */) ),
  DEBUG_EVENT( DBTEnv::getBooleanProperty("direct_bt.debug.mgmt.event", false) ),
  MGMT_READ_PACKET_MAX_RETRY( MGMT_EVT_RING_CAPACITY )
{
}

const pid_t DBTManager::pidSelf = getpid();
std::mutex DBTManager::mtx_singleton;

void DBTManager::mgmtReaderThreadImpl() {
    {
        const std::lock_guard<std::mutex> lock(mtx_mgmtReaderInit); // RAII-style acquire and relinquish via destructor
        mgmtReaderShallStop = false;
        mgmtReaderRunning = true;
        DBG_PRINT("DBTManager::reader: Started");
        cv_mgmtReaderInit.notify_all();
    }

    while( !mgmtReaderShallStop ) {
        int len;
        if( !comm.isOpen() ) {
            // not open
            ERR_PRINT("DBTManager::reader: Not connected");
            mgmtReaderShallStop = true;
            break;
        }

        len = comm.read(rbuffer.get_wptr(), rbuffer.getSize(), env.MGMT_READER_THREAD_POLL_TIMEOUT);
        if( 0 < len ) {
            const uint16_t paramSize = len >= 6 ? rbuffer.get_uint16(4) : 0;
            if( len < 6 + paramSize ) {
                WARN_PRINT("DBTManager::reader: length mismatch %d < 6 + %d", len, paramSize);
                continue; // discard data
            }
            std::shared_ptr<MgmtEvent> event( MgmtEvent::getSpecialized(rbuffer.get_ptr(), len) );
            const MgmtEvent::Opcode opc = event->getOpcode();
            if( MgmtEvent::Opcode::CMD_COMPLETE == opc || MgmtEvent::Opcode::CMD_STATUS == opc ) {
                COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO RECV (CMD) %s", event->toString().c_str());
                if( mgmtEventRing.isFull() ) {
                    const int dropCount = mgmtEventRing.capacity()/4;
                    mgmtEventRing.drop(dropCount);
                    WARN_PRINT("DBTManager-IO RECV Drop (%d oldest elements of %d capacity, ring full)", dropCount, mgmtEventRing.capacity());
                }
                mgmtEventRing.putBlocking( event );
            } else {
                // issue a callback
                COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO RECV (CB) %s", event->toString().c_str());
                sendMgmtEvent(event);
            }
        } else if( ETIMEDOUT != errno && !mgmtReaderShallStop ) { // expected exits
            ERR_PRINT("DBTManager::reader: HCIComm read error");
        }
    }

    INFO_PRINT("DBTManager::reader: Ended. Ring has %d entries flushed", mgmtEventRing.getSize());
    mgmtReaderRunning = false;
    mgmtEventRing.clear();
}

void DBTManager::sendMgmtEvent(std::shared_ptr<MgmtEvent> event) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    const int dev_id = event->getDevID();
    MgmtAdapterEventCallbackList & mgmtEventCallbackList = mgmtAdapterEventCallbackLists[static_cast<uint16_t>(event->getOpcode())];
    int invokeCount = 0;
    for (auto it = mgmtEventCallbackList.begin(); it != mgmtEventCallbackList.end(); ++it) {
        if( 0 > it->getDevID() || dev_id == it->getDevID() ) {
            try {
                it->getCallback().invoke(event);
            } catch (std::exception &e) {
                ERR_PRINT("DBTManager::sendMgmtEvent-CBs %d/%zd: MgmtAdapterEventCallback %s : Caught exception %s",
                        invokeCount+1, mgmtEventCallbackList.size(),
                        it->toString().c_str(), e.what());
            }
            invokeCount++;
        }
    }
    COND_PRINT(env.DEBUG_EVENT, "DBTManager::sendMgmtEvent: Event %s -> %d/%zd callbacks", event->toString().c_str(), invokeCount, mgmtEventCallbackList.size());
    (void)invokeCount;
}

static void mgmthandler_sigaction(int sig, siginfo_t *info, void *ucontext) {
    bool pidMatch = info->si_pid == DBTManager::pidSelf;
    INFO_PRINT("DBTManager.sigaction: sig %d, info[code %d, errno %d, signo %d, pid %d, uid %d, fd %d], pid-self %d (match %d)",
            sig, info->si_code, info->si_errno, info->si_signo,
            info->si_pid, info->si_uid, info->si_fd,
            DBTManager::pidSelf, pidMatch);
    (void)ucontext;

    if( !pidMatch || SIGALRM != sig ) {
        return;
    }
#if 0
    // We do not de-install the handler on single use,
    // as we act for multiple SIGALRM events within direct-bt
    {
        struct sigaction sa_setup;
        bzero(&sa_setup, sizeof(sa_setup));
        sa_setup.sa_handler = SIG_DFL;
        sigemptyset(&(sa_setup.sa_mask));
        sa_setup.sa_flags = 0;
        if( 0 != sigaction( SIGALRM, &sa_setup, NULL ) ) {
            ERR_PRINT("DBTManager.sigaction: Resetting sighandler");
        }
    }
#endif
}

std::shared_ptr<MgmtEvent> DBTManager::sendWithReply(MgmtCommand &req) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_sendReply); // RAII-style acquire and relinquish via destructor
    {
        COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO SENT %s", req.toString().c_str());
        TROOctets & pdu = req.getPDU();
        if ( comm.write( pdu.get_ptr(), pdu.getSize() ) < 0 ) {
            ERR_PRINT("DBTManager::sendWithReply: HCIComm write error, req %s", req.toString().c_str());
            return nullptr;
        }
    }

    // Ringbuffer read is thread safe
    int32_t retryCount = 0;
    while( retryCount < env.MGMT_READ_PACKET_MAX_RETRY ) {
        std::shared_ptr<MgmtEvent> res = mgmtEventRing.getBlocking(env.MGMT_COMMAND_REPLY_TIMEOUT);
        // std::shared_ptr<MgmtEvent> res = receiveNext();
        if( nullptr == res ) {
            errno = ETIMEDOUT;
            ERR_PRINT("DBTManager::sendWithReply.X: nullptr result (timeout -> abort): req %s", req.toString().c_str());
            return nullptr;
        } else if( !res->validate(req) ) {
            // This could occur due to an earlier timeout w/ a nullptr == res (see above),
            // i.e. the pending reply processed here and naturally not-matching.
            COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO RECV sendWithReply: res mismatch (drop evt, retryCount %d): res %s; req %s",
                    retryCount, res->toString().c_str(), req.toString().c_str());
            retryCount++;
        } else {
            COND_PRINT(env.DEBUG_EVENT, "DBTManager-IO RECV sendWithReply: res %s; req %s", res->toString().c_str(), req.toString().c_str());
            return res;
        }
    }
    return nullptr;
}

void DBTManager::setAdapterMode(const uint16_t dev_id, const uint8_t ssp, const uint8_t bredr, const uint8_t le) {
    bool res;
    res = setMode(dev_id, MgmtOpcode::SET_SSP, ssp);
    DBG_PRINT("setAdapterMode[%d]: SET_SSP(%d): result %d", dev_id, ssp, res);

    res = setMode(dev_id, MgmtOpcode::SET_BREDR, bredr);
    DBG_PRINT("setAdapterMode[%d]: SET_BREDR(%d): result %d", dev_id, bredr, res);

    res = setMode(dev_id, MgmtOpcode::SET_LE, le);
    DBG_PRINT("setAdapterMode[%d]: SET_LE(%d): result %d", dev_id, le, res);
}

std::shared_ptr<AdapterInfo> DBTManager::initAdapter(const uint16_t dev_id, const BTMode btMode) {
    std::shared_ptr<AdapterInfo> adapterInfo = nullptr;
    bool powered;
    MgmtCommand req0(MgmtOpcode::READ_INFO, dev_id);
    {
        std::shared_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto fail;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE != res->getOpcode() || res->getTotalSize() < MgmtEvtAdapterInfo::getRequiredSize()) {
            ERR_PRINT("Insufficient data for adapter info: req %d, res %s", MgmtEvtAdapterInfo::getRequiredSize(), res->toString().c_str());
            goto fail;
        }
        const MgmtEvtAdapterInfo * res1 = static_cast<MgmtEvtAdapterInfo*>(res.get());
        adapterInfo = res1->toAdapterInfo();
        if( dev_id != adapterInfo->dev_id ) {
            throw InternalError("AdapterInfo dev_id="+std::to_string(adapterInfo->dev_id)+" != dev_id="+std::to_string(dev_id)+"]: "+adapterInfo->toString(), E_FILE_LINE);
        }
    }
    DBG_PRINT("initAdapter[%d]: Start: %s", dev_id, adapterInfo->toString().c_str());

    switch ( btMode ) {
        case BTMode::DUAL:
            setAdapterMode(dev_id, 1 /* ssp */, 1 /* bredr */, 1 /* le */);
            break;
        case BTMode::BREDR:
            setAdapterMode(dev_id, 1 /* ssp */, 1 /* bredr */, 0 /* le */);
            break;
        case BTMode::NONE:
            // fall through intended, map NONE -> LE
        case BTMode::LE:
            setAdapterMode(dev_id, 0 /* ssp */, 0 /* bredr */, 1 /* le */);
            break;
    }

    setMode(dev_id, MgmtOpcode::SET_CONNECTABLE, 0);
    setMode(dev_id, MgmtOpcode::SET_FAST_CONNECTABLE, 0);

    removeDeviceFromWhitelist(dev_id, EUI48_ANY_DEVICE, BDAddressType::BDADDR_BREDR); // flush whitelist!

    powered = setMode(dev_id, MgmtOpcode::SET_POWERED, 1);
    DBG_PRINT("setAdapterMode[%d]: SET_POWERED(1): result %d", dev_id, powered);

    /**
     * Update AdapterSettings post settings
     */
    {
        adapterInfo = nullptr; // flush

        std::shared_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto fail;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE != res->getOpcode() || res->getTotalSize() < MgmtEvtAdapterInfo::getRequiredSize()) {
            ERR_PRINT("Insufficient data for adapter info: req %d, res %s", MgmtEvtAdapterInfo::getRequiredSize(), res->toString().c_str());
            goto fail;
        }
        const MgmtEvtAdapterInfo * res1 = static_cast<MgmtEvtAdapterInfo*>(res.get());
        adapterInfo = res1->toAdapterInfo();
        if( dev_id != adapterInfo->dev_id ) {
            throw InternalError("AdapterInfo dev_id="+std::to_string(adapterInfo->dev_id)+" != dev_id="+std::to_string(dev_id)+"]: "+adapterInfo->toString(), E_FILE_LINE);
        }
    }
    DBG_PRINT("initAdapter[%d]: End: %s", dev_id, adapterInfo->toString().c_str());

fail:
    return adapterInfo;
}

void DBTManager::shutdownAdapter(const uint16_t dev_id) {
    setMode(dev_id, MgmtOpcode::SET_CONNECTABLE, 0);
    setMode(dev_id, MgmtOpcode::SET_FAST_CONNECTABLE, 0);
    setMode(dev_id, MgmtOpcode::SET_DISCOVERABLE, 0);
    setMode(dev_id, MgmtOpcode::SET_POWERED, 0);
}

DBTManager::DBTManager(const BTMode _defaultBTMode)
: env(MgmtEnv::get()),
  defaultBTMode(BTMode::NONE != _defaultBTMode ? _defaultBTMode : BTMode::LE),
  rbuffer(ClientMaxMTU), comm(HCI_DEV_NONE, HCI_CHANNEL_CONTROL),
  mgmtEventRing(env.MGMT_EVT_RING_CAPACITY), mgmtReaderRunning(false), mgmtReaderShallStop(false)
{
    INFO_PRINT("DBTManager.ctor: pid %d", DBTManager::pidSelf);
    if( !comm.isOpen() ) {
        ERR_PRINT("DBTManager::open: Could not open mgmt control channel");
        return;
    }

    {
        struct sigaction sa_setup;
        bzero(&sa_setup, sizeof(sa_setup));
        sa_setup.sa_sigaction = mgmthandler_sigaction;
        sigemptyset(&(sa_setup.sa_mask));
        sa_setup.sa_flags = SA_SIGINFO;
        if( 0 != sigaction( SIGALRM, &sa_setup, NULL ) ) {
            ERR_PRINT("DBTManager::ctor: Setting sighandler");
        }
    }
    {
        std::unique_lock<std::mutex> lock(mtx_mgmtReaderInit); // RAII-style acquire and relinquish via destructor
        mgmtReaderThread = std::thread(&DBTManager::mgmtReaderThreadImpl, this);
        while( false == mgmtReaderRunning ) {
            cv_mgmtReaderInit.wait(lock);
        }
    }

    PERF_TS_T0();

    bool ok = true;
    // Mandatory
    {
        MgmtCommand req0(MgmtOpcode::READ_VERSION, MgmtConstU16::MGMT_INDEX_NONE);
        std::shared_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto fail;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE != res->getOpcode() || res->getDataSize() < 3) {
            ERR_PRINT("Wrong version response: %s", res->toString().c_str());
            goto fail;
        }
        const uint8_t *data = res->getData();
        const uint8_t version = data[0];
        const uint16_t revision = get_uint16(data, 1, true /* littleEndian */);
        INFO_PRINT("Bluetooth version %d.%d", version, revision);
        if( version < 1 ) {
            ERR_PRINT("Bluetooth version >= 1.0 required");
            goto fail;
        }
    }
    // Optional
    {
        MgmtCommand req0(MgmtOpcode::READ_COMMANDS, MgmtConstU16::MGMT_INDEX_NONE);
        std::shared_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto next1;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE == res->getOpcode() && res->getDataSize() >= 4) {
            const uint8_t *data = res->getData();
            const uint16_t num_commands = get_uint16(data, 0, true /* littleEndian */);
            const uint16_t num_events = get_uint16(data, 2, true /* littleEndian */);
            INFO_PRINT("Bluetooth %d commands, %d events", num_commands, num_events);
#ifdef VERBOSE_ON
            const int expDataSize = 4 + num_commands * 2 + num_events * 2;
            if( res->getDataSize() >= expDataSize ) {
                for(int i=0; i< num_commands; i++) {
                    const MgmtOpcode op = static_cast<MgmtOpcode>( get_uint16(data, 4+i*2, true /* littleEndian */) );
                    DBG_PRINT("kernel op %d: %s", i, getMgmtOpcodeString(op).c_str());
                }
            }
#endif
        }
    }

next1:
    // Register to add/remove adapter optionally:
    // MgmtEvent::INDEX_ADDED, MgmtConst::INDEX_NONE;
    // MgmtEvent::INDEX_REMOVED, MgmtConst::INDEX_NONE;

    // Mandatory
    {
        MgmtCommand req0(MgmtOpcode::READ_INDEX_LIST, MgmtConstU16::MGMT_INDEX_NONE);
        std::shared_ptr<MgmtEvent> res = sendWithReply(req0);
        if( nullptr == res ) {
            goto fail;
        }
        if( MgmtEvent::Opcode::CMD_COMPLETE != res->getOpcode() || res->getDataSize() < 2) {
            ERR_PRINT("Insufficient data for adapter index: res %s", res->toString().c_str());
            goto fail;
        }
        const uint8_t *data = res->getData();
        const uint16_t num_adapter = get_uint16(data, 0, true /* littleEndian */);
        INFO_PRINT("Bluetooth %d adapter", num_adapter);

        const int expDataSize = 2 + num_adapter * 2;
        if( res->getDataSize() < expDataSize ) {
            ERR_PRINT("Insufficient data for %d adapter indices: res %s", num_adapter, res->toString().c_str());
            goto fail;
        }
        adapterInfos.resize(num_adapter, nullptr);
        for(int i=0; ok && i < num_adapter; i++) {
            const uint16_t dev_id = get_uint16(data, 2+i*2, true /* littleEndian */);
            if( dev_id >= num_adapter ) {
                throw InternalError("dev_id "+std::to_string(dev_id)+" >= num_adapter "+std::to_string(num_adapter), E_FILE_LINE);
            }
            if( adapterInfos[dev_id] != nullptr ) {
                throw InternalError("adapters[dev_id="+std::to_string(dev_id)+"] != nullptr: "+adapterInfos[dev_id]->toString(), E_FILE_LINE);
            }
            std::shared_ptr<AdapterInfo> adapterInfo = initAdapter(dev_id, defaultBTMode);
            adapterInfos[dev_id] = adapterInfo;
            if( nullptr != adapterInfo ) {
                DBG_PRINT("DBTManager::adapters %d/%d: dev_id %d: %s", i, num_adapter, dev_id, adapterInfo->toString().c_str());
                ok = true;
            } else {
                DBG_PRINT("DBTManager::adapters %d/%d: dev_id %d: FAILED", i, num_adapter, dev_id);
                ok = false;
            }
        }
    }

    if( ok ) {
        if( env.DEBUG_EVENT ) {
            addMgmtEventCallback(-1, MgmtEvent::Opcode::CLASS_OF_DEV_CHANGED, bindMemberFunc(this, &DBTManager::mgmtEvClassOfDeviceChangedCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::DISCOVERING, bindMemberFunc(this, &DBTManager::mgmtEvDeviceDiscoveringCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_FOUND, bindMemberFunc(this, &DBTManager::mgmtEvDeviceFoundCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_DISCONNECTED, bindMemberFunc(this, &DBTManager::mgmtEvDeviceDisconnectedCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_CONNECTED, bindMemberFunc(this, &DBTManager::mgmtEvDeviceConnectedCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::CONNECT_FAILED, bindMemberFunc(this, &DBTManager::mgmtEvConnectFailedCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_BLOCKED, bindMemberFunc(this, &DBTManager::mgmtEvDeviceBlockedCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_UNBLOCKED, bindMemberFunc(this, &DBTManager::mgmtEvDeviceUnblockedCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_UNPAIRED, bindMemberFunc(this, &DBTManager::mgmtEvDeviceUnpairedCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::NEW_CONN_PARAM, bindMemberFunc(this, &DBTManager::mgmtEvNewConnectionParamCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_WHITELIST_ADDED, bindMemberFunc(this, &DBTManager::mgmtEvDeviceWhitelistAddedCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::DEVICE_WHITELIST_REMOVED, bindMemberFunc(this, &DBTManager::mgmtEvDeviceWhilelistRemovedCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::PIN_CODE_REQUEST, bindMemberFunc(this, &DBTManager::mgmtEvPinCodeRequestCB));
            addMgmtEventCallback(-1, MgmtEvent::Opcode::USER_PASSKEY_REQUEST, bindMemberFunc(this, &DBTManager::mgmtEvUserPasskeyRequestCB));
        }
        PERF_TS_TD("DBTManager::open.ok");
        return;
    }

fail:
    close();
    PERF_TS_TD("DBTManager::open.fail");
    return;
}

void DBTManager::close() {
    DBG_PRINT("DBTManager::close: Start");

    removeAllDevicesFromWhitelist();

    clearAllMgmtEventCallbacks();

    for (auto it = adapterInfos.begin(); it != adapterInfos.end(); it++) {
        shutdownAdapter((*it)->dev_id);
    }
    adapterInfos.clear();

    if( mgmtReaderRunning && mgmtReaderThread.joinable() ) {
        mgmtReaderShallStop = true;
        pthread_t tid = mgmtReaderThread.native_handle();
        pthread_kill(tid, SIGALRM);
    }
    comm.close();

    if( mgmtReaderRunning && mgmtReaderThread.joinable() ) {
        // still running ..
        DBG_PRINT("DBTManager::close: join mgmtReaderThread");
        mgmtReaderThread.join();
    }
    mgmtReaderThread = std::thread(); // empty
    {
        struct sigaction sa_setup;
        bzero(&sa_setup, sizeof(sa_setup));
        sa_setup.sa_handler = SIG_DFL;
        sigemptyset(&(sa_setup.sa_mask));
        sa_setup.sa_flags = 0;
        if( 0 != sigaction( SIGALRM, &sa_setup, NULL ) ) {
            ERR_PRINT("DBTManager.sigaction: Resetting sighandler");
        }
    }
    DBG_PRINT("DBTManager::close: End");
}

int DBTManager::findAdapterInfoIdx(const EUI48 &mac) const {
    auto begin = adapterInfos.begin();
    auto it = std::find_if(begin, adapterInfos.end(), [&](std::shared_ptr<AdapterInfo> const& p) {
        return p->address == mac;
    });
    if ( it == std::end(adapterInfos) ) {
        return -1;
    } else {
        return std::distance(begin, it);
    }
}
std::shared_ptr<AdapterInfo> DBTManager::findAdapterInfo(const EUI48 &mac) const {
    auto begin = adapterInfos.begin();
    auto it = std::find_if(begin, adapterInfos.end(), [&](std::shared_ptr<AdapterInfo> const& p) {
        return p->address == mac;
    });
    if ( it == std::end(adapterInfos) ) {
        return nullptr;
    } else {
        return *it;
    }
}
std::shared_ptr<AdapterInfo> DBTManager::getAdapterInfo(const int idx) const {
    if( 0 > idx || idx >= static_cast<int>(adapterInfos.size()) ) {
        throw IndexOutOfBoundsException(idx, adapterInfos.size(), 1, E_FILE_LINE);
    }
    std::shared_ptr<AdapterInfo> adapter = adapterInfos.at(idx);
    return adapter;
}

bool DBTManager::setMode(const int dev_id, const MgmtOpcode opc, const uint8_t mode) {
    MgmtUint8Cmd req(opc, dev_id, mode);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res ) {
        if( res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
            const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
            return MgmtStatus::SUCCESS == res1.getStatus();
        } else if( res->getOpcode() == MgmtEvent::Opcode::CMD_STATUS ) {
            const MgmtEvtCmdStatus &res1 = *static_cast<const MgmtEvtCmdStatus *>(res.get());
            return MgmtStatus::SUCCESS == res1.getStatus();
        }
    }
    return false;
}

ScanType DBTManager::startDiscovery(const int dev_id, const BTMode btMode) {
    return startDiscovery(dev_id, getScanType(btMode));
}

ScanType DBTManager::startDiscovery(const int dev_id, const ScanType scanType) {
    MgmtUint8Cmd req(MgmtOpcode::START_DISCOVERY, dev_id, number(scanType));
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    ScanType type = ScanType::NONE;
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() && 1 == res1.getDataSize() ) {
            type = static_cast<ScanType>( *res1.getData() );
        }
    }
    return type;
}
bool DBTManager::stopDiscovery(const int dev_id, const ScanType type) {
    MgmtUint8Cmd req(MgmtOpcode::STOP_DISCOVERY, dev_id, number(type));
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        return MgmtStatus::SUCCESS == res1.getStatus();
    }
    return false;
}

bool DBTManager::uploadConnParam(const int dev_id, const EUI48 &address, const BDAddressType address_type,
                                 const uint16_t min_interval, const uint16_t max_interval,
                                 const uint16_t latency, const uint16_t timeout) {
    MgmtConnParam connParam{ address, address_type, min_interval, max_interval, latency, timeout };
    MgmtLoadConnParamCmd req(dev_id, connParam);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        return MgmtStatus::SUCCESS == res1.getStatus();
    }
    return false;
}

bool DBTManager::isDeviceWhitelisted(const int dev_id, const EUI48 &address) {
    for(auto it = whitelist.begin(); it != whitelist.end(); ) {
        std::shared_ptr<WhitelistElem> wle = *it;
        if( wle->dev_id == dev_id && wle->address == address ) {
            return true;
        } else {
            ++it;
        }
    }
    return false;
}

bool DBTManager::addDeviceToWhitelist(const int dev_id, const EUI48 &address, const BDAddressType address_type, const HCIWhitelistConnectType ctype) {
    MgmtAddDeviceToWhitelistCmd req(dev_id, address, address_type, ctype);

    // Check if already exist in our local whitelist first, reject if so ..
    if( isDeviceWhitelisted(dev_id, address) ) {
        ERR_PRINT("DBTManager::addDeviceToWhitelist: Already in local whitelist, remove first: %s", req.toString().c_str());
        return false;
    }
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            std::shared_ptr<WhitelistElem> wle( new WhitelistElem{dev_id, address, address_type, ctype} );
            whitelist.push_back(wle);
            return true;
        }
    }
    return false;
}

int DBTManager::removeAllDevicesFromWhitelist() {
#if 0
    std::vector<std::shared_ptr<WhitelistElem>> whitelist_copy = whitelist;
    int count = 0;
    DBG_PRINT("DBTManager::removeAllDevicesFromWhitelist.A: Start %zd elements", whitelist_copy.size());

    for(auto it = whitelist_copy.begin(); it != whitelist_copy.end(); ++it) {
        std::shared_ptr<WhitelistElem> wle = *it;
        removeDeviceFromWhitelist(wle->dev_id, wle->address, wle->address_type);
        count++;
    }
#else
    int count = whitelist.size();
    DBG_PRINT("DBTManager::removeAllDevicesFromWhitelist.B: Start %d elements", count);
    whitelist.clear();
    for (auto it = adapterInfos.begin(); it != adapterInfos.end(); it++) {
        removeDeviceFromWhitelist((*it)->dev_id, EUI48_ANY_DEVICE, BDAddressType::BDADDR_BREDR); // flush whitelist!
    }
#endif

    DBG_PRINT("DBTManager::removeAllDevicesFromWhitelist: End: Removed %d elements, remaining %zd elements",
            count, whitelist.size());
    return count;
}

bool DBTManager::removeDeviceFromWhitelist(const int dev_id, const EUI48 &address, const BDAddressType address_type) {
    // Remove from our local whitelist first
    {
        for(auto it = whitelist.begin(); it != whitelist.end(); ) {
            std::shared_ptr<WhitelistElem> wle = *it;
            if( wle->dev_id == dev_id && wle->address == address ) {
                it = whitelist.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Actual removal
    MgmtRemoveDeviceFromWhitelistCmd req(dev_id, address, address_type);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            return true;
        }
    }
    return false;
}

bool DBTManager::disconnect(const bool ioErrorCause,
                            const int dev_id, const EUI48 &peer_bdaddr, const BDAddressType peer_mac_type,
                            const HCIStatusCode reason) {
    bool bres = false;

    if( !ioErrorCause ) {
        MgmtDisconnectCmd req(dev_id, peer_bdaddr, peer_mac_type);
        std::shared_ptr<MgmtEvent> res = sendWithReply(req);
        if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
            const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
            if( MgmtStatus::SUCCESS == res1.getStatus() ) {
                bres = true;
            }
        }
    } else {
        // explicit disconnected event anyways
        MgmtEvtDeviceDisconnected *e = new MgmtEvtDeviceDisconnected(dev_id, peer_bdaddr, peer_mac_type, reason, 0xffff);
        sendMgmtEvent(std::shared_ptr<MgmtEvent>(e));
    }
    return bres;
}

std::shared_ptr<ConnectionInfo> DBTManager::getConnectionInfo(const int dev_id, const EUI48 &address, const BDAddressType address_type) {
    MgmtGetConnectionInfoCmd req(dev_id, address, address_type);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            std::shared_ptr<ConnectionInfo> result = res1.toConnectionInfo();
            return result;
        }
    }
    return nullptr;
}

std::shared_ptr<NameAndShortName> DBTManager::setLocalName(const int dev_id, const std::string & name, const std::string & short_name) {
    MgmtSetLocalNameCmd req (dev_id, name, short_name);
    std::shared_ptr<MgmtEvent> res = sendWithReply(req);
    if( nullptr != res && res->getOpcode() == MgmtEvent::Opcode::CMD_COMPLETE ) {
        const MgmtEvtCmdComplete &res1 = *static_cast<const MgmtEvtCmdComplete *>(res.get());
        if( MgmtStatus::SUCCESS == res1.getStatus() ) {
            std::shared_ptr<NameAndShortName> result = res1.toNameAndShortName();

            // explicit LocalNameChanged event
            MgmtEvtLocalNameChanged * e = new MgmtEvtLocalNameChanged(dev_id, result->getName(), result->getShortName());
            sendMgmtEvent(std::shared_ptr<MgmtEvent>(e));
            return result;
        }
    }
    return nullptr;
}

/***
 *
 * MgmtEventCallback section
 *
 */

void DBTManager::addMgmtEventCallback(const int dev_id, const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    checkMgmtEventCallbackListsIndex(opc);
    MgmtAdapterEventCallbackList &l = mgmtAdapterEventCallbackLists[static_cast<uint16_t>(opc)];
    for (auto it = l.begin(); it != l.end(); ++it) {
        if ( it->getDevID() == dev_id && it->getCallback() == cb ) {
            // already exists for given adapter
            return;
        }
    }
    l.push_back( MgmtAdapterEventCallback(dev_id, cb) );
}
int DBTManager::removeMgmtEventCallback(const MgmtEvent::Opcode opc, const MgmtEventCallback &cb) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    checkMgmtEventCallbackListsIndex(opc);
    int count = 0;
    MgmtAdapterEventCallbackList &l = mgmtAdapterEventCallbackLists[static_cast<uint16_t>(opc)];
    for (auto it = l.begin(); it != l.end(); ) {
        if ( it->getCallback() == cb ) {
            it = l.erase(it);
            count++;
        } else {
            ++it;
        }
    }
    return count;
}
int DBTManager::removeMgmtEventCallback(const int dev_id) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    int count = 0;
    for(size_t i=0; i<mgmtAdapterEventCallbackLists.size(); i++) {
        MgmtAdapterEventCallbackList &l = mgmtAdapterEventCallbackLists[i];
        for (auto it = l.begin(); it != l.end(); ) {
            if ( it->getDevID() == dev_id ) {
                it = l.erase(it);
                count++;
            } else {
                ++it;
            }
        }
    }
    return count;
}
void DBTManager::clearMgmtEventCallbacks(const MgmtEvent::Opcode opc) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    checkMgmtEventCallbackListsIndex(opc);
    mgmtAdapterEventCallbackLists[static_cast<uint16_t>(opc)].clear();
}
void DBTManager::clearAllMgmtEventCallbacks() {
    const std::lock_guard<std::recursive_mutex> lock(mtx_callbackLists); // RAII-style acquire and relinquish via destructor
    for(size_t i=0; i<mgmtAdapterEventCallbackLists.size(); i++) {
        mgmtAdapterEventCallbackLists[i].clear();
    }
}

bool DBTManager::mgmtEvClassOfDeviceChangedCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:ClassOfDeviceChanged: %s", e->toString().c_str());
    (void)e;
    return true;
}
bool DBTManager::mgmtEvDeviceDiscoveringCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:DeviceDiscovering: %s", e->toString().c_str());
    const MgmtEvtDiscovering &event = *static_cast<const MgmtEvtDiscovering *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceFoundCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:DeviceFound: %s", e->toString().c_str());
    const MgmtEvtDeviceFound &event = *static_cast<const MgmtEvtDeviceFound *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceDisconnectedCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:DeviceDisconnected: %s", e->toString().c_str());
    const MgmtEvtDeviceDisconnected &event = *static_cast<const MgmtEvtDeviceDisconnected *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceConnectedCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:DeviceConnected: %s", e->toString().c_str());
    const MgmtEvtDeviceConnected &event = *static_cast<const MgmtEvtDeviceConnected *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvConnectFailedCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:ConnectFailed: %s", e->toString().c_str());
    const MgmtEvtDeviceConnectFailed &event = *static_cast<const MgmtEvtDeviceConnectFailed *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceBlockedCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:DeviceBlocked: %s", e->toString().c_str());
    const MgmtEvtDeviceBlocked &event = *static_cast<const MgmtEvtDeviceBlocked *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceUnblockedCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:DeviceUnblocked: %s", e->toString().c_str());
    const MgmtEvtDeviceUnblocked &event = *static_cast<const MgmtEvtDeviceUnblocked *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceUnpairedCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:DeviceUnpaired: %s", e->toString().c_str());
    const MgmtEvtDeviceUnpaired &event = *static_cast<const MgmtEvtDeviceUnpaired *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvNewConnectionParamCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:NewConnectionParam: %s", e->toString().c_str());
    const MgmtEvtNewConnectionParam &event = *static_cast<const MgmtEvtNewConnectionParam *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceWhitelistAddedCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:DeviceWhitelistAdded: %s", e->toString().c_str());
    const MgmtEvtDeviceWhitelistAdded &event = *static_cast<const MgmtEvtDeviceWhitelistAdded *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvDeviceWhilelistRemovedCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:DeviceWhitelistRemoved: %s", e->toString().c_str());
    const MgmtEvtDeviceWhitelistRemoved &event = *static_cast<const MgmtEvtDeviceWhitelistRemoved *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvPinCodeRequestCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:PinCodeRequest: %s", e->toString().c_str());
    const MgmtEvtPinCodeRequest &event = *static_cast<const MgmtEvtPinCodeRequest *>(e.get());
    (void)event;
    return true;
}
bool DBTManager::mgmtEvUserPasskeyRequestCB(std::shared_ptr<MgmtEvent> e) {
    PLAIN_PRINT("DBTManager::EventCB:UserPasskeyRequest: %s", e->toString().c_str());
    const MgmtEvtUserPasskeyRequest &event = *static_cast<const MgmtEvtUserPasskeyRequest *>(e.get());
    (void)event;
    return true;
}
