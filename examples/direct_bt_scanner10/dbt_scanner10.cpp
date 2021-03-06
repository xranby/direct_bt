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

#include <direct_bt/DirectBT.hpp>
#include <cinttypes>

#include "direct_bt/dfa_utf8_decode.hpp"

extern "C" {
    #include <unistd.h>
}

using namespace direct_bt;

/**
 * This C++ scanner example uses the Direct-BT fully event driven workflow
 * and adds multithreading, i.e. one thread processes each found device found
 * as notified via the event listener.
 * <p>
 * This example represents the recommended utilization of Direct-BT.
 * </p>
 */

static int64_t timestamp_t0;

static int MULTI_MEASUREMENTS = 8;

static bool KEEP_CONNECTED = true;
static bool REMOVE_DEVICE = true;

static bool USE_WHITELIST = false;
static std::vector<std::shared_ptr<EUI48>> WHITELIST;

static bool SHOW_UPDATE_EVENTS = false;

static EUI48 waitForDevice = EUI48_ANY_DEVICE;

static void connectDiscoveredDevice(std::shared_ptr<DBTDevice> device);

static void processConnectedDevice(std::shared_ptr<DBTDevice> device);

#include <pthread.h>

static std::vector<EUI48> devicesInProcessing;
static std::recursive_mutex mtx_devicesProcessing;

static std::recursive_mutex mtx_devicesProcessed;
static std::vector<EUI48> devicesProcessed;

static void addToDevicesProcessed(const EUI48 &a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    devicesProcessed.push_back(a);
}
static bool isDeviceProcessed(const EUI48 & a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessed); // RAII-style acquire and relinquish via destructor
    for (auto it = devicesProcessed.begin(); it != devicesProcessed.end(); ++it) {
        if ( a == *it ) {
            return true;
        }
    }
    return false;
}

static void addToDevicesProcessing(const EUI48 &a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    devicesInProcessing.push_back(a);
}
static bool removeFromDevicesProcessing(const EUI48 &a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    for (auto it = devicesInProcessing.begin(); it != devicesInProcessing.end(); ++it) {
        if ( a == *it ) {
            devicesInProcessing.erase(it);
            return true;
        }
    }
    return false;
}
static bool isDeviceProcessing(const EUI48 & a) {
    const std::lock_guard<std::recursive_mutex> lock(mtx_devicesProcessing); // RAII-style acquire and relinquish via destructor
    for (auto it = devicesInProcessing.begin(); it != devicesInProcessing.end(); ++it) {
        if ( a == *it ) {
            return true;
        }
    }
    return false;
}

class MyAdapterStatusListener : public AdapterStatusListener {

    void adapterSettingsChanged(DBTAdapter const &a, const AdapterSetting oldmask, const AdapterSetting newmask,
                                const AdapterSetting changedmask, const uint64_t timestamp) override {
        fprintf(stderr, "****** SETTINGS_CHANGED: %s -> %s, changed %s\n",
                getAdapterSettingsString(oldmask).c_str(),
                getAdapterSettingsString(newmask).c_str(),
                getAdapterSettingsString(changedmask).c_str());
        fprintf(stderr, "Status DBTAdapter:\n");
        fprintf(stderr, "%s\n", a.toString().c_str());
        (void)timestamp;
    }

    void discoveringChanged(DBTAdapter const &a, const bool enabled, const bool keepAlive, const uint64_t timestamp) override {
        fprintf(stderr, "****** DISCOVERING: enabled %d, keepAlive %d: %s\n", enabled, keepAlive, a.toString().c_str());
        (void)timestamp;
    }

    void deviceFound(std::shared_ptr<DBTDevice> device, const uint64_t timestamp) override {
        (void)timestamp;

        if( BDAddressType::BDADDR_LE_PUBLIC != device->getAddressType()
            && BLERandomAddressType::STATIC_PUBLIC != device->getBLERandomAddressType() ) {
            fprintf(stderr, "****** FOUND__-2: Skip 'non public' or 'random static public' LE %s\n", device->toString(true).c_str());
            return;
        }
        if( !isDeviceProcessing( device->getAddress() ) &&
            ( waitForDevice == EUI48_ANY_DEVICE ||
              ( waitForDevice == device->getAddress() &&
                ( 0 < MULTI_MEASUREMENTS || !isDeviceProcessed(waitForDevice) )
              )
            )
          )
        {
            fprintf(stderr, "****** FOUND__-0: Connecting %s\n", device->toString(true).c_str());
            {
                const uint64_t td = getCurrentMilliseconds() - timestamp_t0; // adapter-init -> now
                fprintf(stderr, "PERF: adapter-init -> FOUND__-0  %" PRIu64 " ms\n", td);
            }
            std::thread dc(::connectDiscoveredDevice, device);
            dc.detach();
        } else {
            fprintf(stderr, "****** FOUND__-1: NOP %s\n", device->toString(true).c_str());
        }
    }

    void deviceUpdated(std::shared_ptr<DBTDevice> device, const EIRDataType updateMask, const uint64_t timestamp) override {
        if( SHOW_UPDATE_EVENTS ) {
            fprintf(stderr, "****** UPDATED: %s of %s\n", getEIRDataMaskString(updateMask).c_str(), device->toString(true).c_str());
        }
        (void)timestamp;
    }

    void deviceConnected(std::shared_ptr<DBTDevice> device, const uint16_t handle, const uint64_t timestamp) override {
        (void)handle;
        (void)timestamp;

        if( !isDeviceProcessing( device->getAddress() ) &&
            ( waitForDevice == EUI48_ANY_DEVICE ||
              ( waitForDevice == device->getAddress() &&
                ( 0 < MULTI_MEASUREMENTS || !isDeviceProcessed(waitForDevice) )
              )
            )
          )
        {
            fprintf(stderr, "****** CONNECTED-0: Processing %s\n", device->toString(true).c_str());
            {
                const uint64_t td = getCurrentMilliseconds() - timestamp_t0; // adapter-init -> now
                fprintf(stderr, "PERF: adapter-init -> CONNECTED-0  %" PRIu64 " ms\n", td);
            }
            addToDevicesProcessing(device->getAddress());
            std::thread dc(::processConnectedDevice, device);
            dc.detach();
        } else {
            fprintf(stderr, "****** CONNECTED-1: NOP %s\n", device->toString(true).c_str());
        }
    }
    void deviceDisconnected(std::shared_ptr<DBTDevice> device, const HCIStatusCode reason, const uint16_t handle, const uint64_t timestamp) override {
        fprintf(stderr, "****** DISCONNECTED: Reason 0x%X (%s), old handle %s: %s\n",
                static_cast<uint8_t>(reason), getHCIStatusCodeString(reason).c_str(),
                uint16HexString(handle).c_str(), device->toString(true).c_str());
        (void)timestamp;
    }

    std::string toString() const override {
        return "MyAdapterStatusListener[this "+aptrHexString(this)+"]";
    }
};

static const uuid16_t _TEMPERATURE_MEASUREMENT(GattCharacteristicType::TEMPERATURE_MEASUREMENT);

class MyGATTEventListener : public AssociatedGATTCharacteristicListener {
  public:

    MyGATTEventListener(const GATTCharacteristic * characteristicMatch)
    : AssociatedGATTCharacteristicListener(characteristicMatch) {}

    void notificationReceived(GATTCharacteristicRef charDecl, std::shared_ptr<TROOctets> charValue, const uint64_t timestamp) override {
        const std::shared_ptr<DBTDevice> dev = charDecl->getDeviceChecked();
        const int64_t tR = getCurrentMilliseconds();
        fprintf(stderr, "****** GATT Notify (td %" PRIu64 " ms, dev-discovered %" PRIu64 " ms): From %s\n",
                (tR-timestamp), (tR-dev->getLastDiscoveryTimestamp()), dev->toString().c_str());
        if( nullptr != charDecl ) {
            fprintf(stderr, "****** decl %s\n", charDecl->toString().c_str());
        }
        fprintf(stderr, "****** rawv %s\n", charValue->toString().c_str());
    }

    void indicationReceived(GATTCharacteristicRef charDecl,
                            std::shared_ptr<TROOctets> charValue, const uint64_t timestamp,
                            const bool confirmationSent) override
    {
        const std::shared_ptr<DBTDevice> dev = charDecl->getDeviceChecked();
        const int64_t tR = getCurrentMilliseconds();
        fprintf(stderr, "****** GATT Indication (confirmed %d, td(msg %" PRIu64 " ms, dev-discovered %" PRIu64 " ms): From %s\n",
                confirmationSent, (tR-timestamp), (tR-dev->getLastDiscoveryTimestamp()), dev->toString().c_str());
        if( nullptr != charDecl ) {
            fprintf(stderr, "****** decl %s\n", charDecl->toString().c_str());
            if( _TEMPERATURE_MEASUREMENT == *charDecl->value_type ) {
                std::shared_ptr<TemperatureMeasurementCharateristic> temp = TemperatureMeasurementCharateristic::get(*charValue);
                if( nullptr != temp ) {
                    fprintf(stderr, "****** valu %s\n", temp->toString().c_str());
                }
            }
        }
        fprintf(stderr, "****** rawv %s\n", charValue->toString().c_str());
    }
};

static void connectDiscoveredDevice(std::shared_ptr<DBTDevice> device) {
    fprintf(stderr, "****** Connecting Device: Start %s\n", device->toString().c_str());
    device->getAdapter().stopDiscovery();
    HCIStatusCode res;
    if( !USE_WHITELIST ) {
        res = device->connectDefault();
    } else {
        res = HCIStatusCode::SUCCESS;
    }
    fprintf(stderr, "****** Connecting Device: End result %s of %s\n", getHCIStatusCodeString(res).c_str(), device->toString().c_str());
    if( !USE_WHITELIST && 0 == devicesInProcessing.size() && HCIStatusCode::SUCCESS != res ) {
        device->getAdapter().startDiscovery( true );
    }
}

static void processConnectedDevice(std::shared_ptr<DBTDevice> device) {
    fprintf(stderr, "****** Processing Device: Start %s\n", device->toString().c_str());
    device->getAdapter().stopDiscovery(); // make sure for pending connections on failed connect*(..) command
    const uint64_t t1 = getCurrentMilliseconds();
    bool success = false;

    //
    // GATT Service Processing
    //
    fprintf(stderr, "****** Processing Device: GATT start: %s\n", device->getAddressString().c_str());
    device->getAdapter().printSharedPtrListOfDevices();
    try {
        std::vector<GATTServiceRef> primServices = device->getGATTServices(); // implicit GATT connect...
        if( 0 == primServices.size() ) {
            fprintf(stderr, "****** Processing Device: getServices() failed %s\n", device->toString().c_str());
            goto exit;
        }

        const uint64_t t5 = getCurrentMilliseconds();
        {
            const uint64_t td01 = t1 - timestamp_t0; // adapter-init -> processing-start
            const uint64_t td15 = t5 - t1; // get-gatt-services
            const uint64_t tdc5 = t5 - device->getLastDiscoveryTimestamp(); // discovered to gatt-complete
            const uint64_t td05 = t5 - timestamp_t0; // adapter-init -> gatt-complete
            fprintf(stderr, "\n\n\n");
            fprintf(stderr, "PERF: GATT primary-services completed\n");
            fprintf(stderr, "PERF:  adapter-init to processing-start %" PRIu64 " ms,\n"
                            "PERF:  get-gatt-services %" PRIu64 " ms,\n"
                            "PERF:  discovered to gatt-complete %" PRIu64 " ms (connect %" PRIu64 " ms),\n"
                            "PERF:  adapter-init to gatt-complete %" PRIu64 " ms\n\n",
                            td01, td15, tdc5, (tdc5 - td15), td05);
        }
        std::shared_ptr<GenericAccess> ga = device->getGATTGenericAccess();
        if( nullptr != ga ) {
            fprintf(stderr, "  GenericAccess: %s\n\n", ga->toString().c_str());
        }
        {
            std::shared_ptr<GATTHandler> gatt = device->getGATTHandler();
            if( nullptr != gatt && gatt->isOpen() ) {
                std::shared_ptr<DeviceInformation> di = gatt->getDeviceInformation(primServices);
                if( nullptr != di ) {
                    fprintf(stderr, "  DeviceInformation: %s\n\n", di->toString().c_str());
                }
            }
        }

        for(size_t i=0; i<primServices.size(); i++) {
            GATTService & primService = *primServices.at(i);
            fprintf(stderr, "  [%2.2d] Service %s\n", (int)i, primService.toString().c_str());
            fprintf(stderr, "  [%2.2d] Service Characteristics\n", (int)i);
            std::vector<GATTCharacteristicRef> & serviceCharacteristics = primService.characteristicList;
            for(size_t j=0; j<serviceCharacteristics.size(); j++) {
                GATTCharacteristic & serviceChar = *serviceCharacteristics.at(j);
                fprintf(stderr, "  [%2.2d.%2.2d] Decla: %s\n", (int)i, (int)j, serviceChar.toString().c_str());
                if( serviceChar.hasProperties(GATTCharacteristic::PropertyBitVal::Read) ) {
                    POctets value(GATTHandler::number(GATTHandler::Defaults::MAX_ATT_MTU), 0);
                    if( serviceChar.readValue(value) ) {
                        std::string sval = dfa_utf8_decode(value.get_ptr(), value.getSize());
                        fprintf(stderr, "  [%2.2d.%2.2d] Value: %s ('%s')\n", (int)i, (int)j, value.toString().c_str(), sval.c_str());
                    }
                }
                bool cccdEnableResult[2];
                bool cccdRet = serviceChar.addCharacteristicListener( std::shared_ptr<GATTCharacteristicListener>( new MyGATTEventListener(&serviceChar) ),
                                                                      cccdEnableResult );
                fprintf(stderr, "  [%2.2d.%2.2d] addCharacteristicListener Notification(%d), Indication(%d): Result %d\n",
                        (int)i, (int)j, cccdEnableResult[0], cccdEnableResult[1], cccdRet);
            }
        }
        // FIXME sleep 1s for potential callbacks ..
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        success = true;
    } catch ( std::exception & e ) {
        fprintf(stderr, "****** Processing Device: Exception caught for %s: %s\n", device->toString().c_str(), e.what());
    }

exit:
    removeFromDevicesProcessing(device->getAddress());
    if( !USE_WHITELIST && 0 == devicesInProcessing.size() ) {
        device->getAdapter().startDiscovery( true );
    }

    if( KEEP_CONNECTED ) {
        while( device->pingGATT() ) {
            fprintf(stderr, "****** Processing Device: pingGATT OK: %s\n", device->getAddressString().c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        fprintf(stderr, "****** Processing Device: pingGATT failed: %s\n", device->getAddressString().c_str());
    }

    fprintf(stderr, "****** Processing Device: disconnecting: %s\n", device->getAddressString().c_str());
    device->disconnect(); // will implicitly purge the GATT data, including GATTCharacteristic listener.
    while( device->getConnected() ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if( REMOVE_DEVICE ) {
        fprintf(stderr, "****** Processing Device: removing: %s\n", device->getAddressString().c_str());
        device->remove();
    }
    device->getAdapter().printSharedPtrListOfDevices();

    if( 0 < MULTI_MEASUREMENTS ) {
        MULTI_MEASUREMENTS--;
        fprintf(stderr, "****** Processing Device: MULTI_MEASUREMENTS left %d: %s\n", MULTI_MEASUREMENTS, device->getAddressString().c_str());
    }
    fprintf(stderr, "****** Processing Device: End: Success %d on %s; devInProc %zd\n",
            success, device->toString().c_str(), devicesInProcessing.size());
    if( success ) {
        addToDevicesProcessed(device->getAddress());
    }
}

void test(int dev_id) {
    bool done = false;

    timestamp_t0 = getCurrentMilliseconds();

    DBTAdapter adapter(dev_id);
    if( !adapter.hasDevId() ) {
        fprintf(stderr, "Default adapter not available.\n");
        exit(1);
    }
    if( !adapter.isValid() ) {
        fprintf(stderr, "Adapter invalid.\n");
        exit(1);
    }
    if( !adapter.isEnabled() ) {
        fprintf(stderr, "Adapter not enabled: device %s, address %s: %s\n",
                adapter.getName().c_str(), adapter.getAddressString().c_str(), adapter.toString().c_str());
        exit(1);
    }
    fprintf(stderr, "Using adapter: device %s, address %s: %s\n",
        adapter.getName().c_str(), adapter.getAddressString().c_str(), adapter.toString().c_str());

    adapter.addStatusListener(std::shared_ptr<AdapterStatusListener>(new MyAdapterStatusListener()));

    if( USE_WHITELIST ) {
        for (auto it = WHITELIST.begin(); it != WHITELIST.end(); ++it) {
            std::shared_ptr<EUI48> wlmac = *it;
            bool res = adapter.addDeviceToWhitelist(*wlmac, BDAddressType::BDADDR_LE_PUBLIC, HCIWhitelistConnectType::HCI_AUTO_CONN_ALWAYS);
            fprintf(stderr, "Added to WHITELIST: res %d, address %s\n", res, wlmac->toString().c_str());
        }
    } else {
        if( !adapter.startDiscovery( true ) ) {
            perror("Adapter start discovery failed");
            done = true;
        }
    }

    while( !done ) {
        if( 0 == MULTI_MEASUREMENTS ||
            ( -1 == MULTI_MEASUREMENTS && waitForDevice != EUI48_ANY_DEVICE && isDeviceProcessed(waitForDevice) )
          )
        {
            fprintf(stderr, "****** EOL Test MULTI_MEASUREMENTS left %d, processed %zd\n",
                    MULTI_MEASUREMENTS, devicesProcessed.size());
            fprintf(stderr, "****** WaitForDevice %s\n", waitForDevice.toString().c_str());
            done = true;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        }
    }
    fprintf(stderr, "****** EOL Adapter's Devices\n");
    adapter.printSharedPtrListOfDevices();
}

int main(int argc, char *argv[])
{
    int dev_id = 0; // default
    BTMode btMode = BTMode::LE; // default
    bool waitForEnter=false;

    for(int i=1; i<argc; i++) {
        if( !strcmp("-wait", argv[i]) ) {
            waitForEnter = true;
        } else if( !strcmp("-show_update_events", argv[i]) ) {
            SHOW_UPDATE_EVENTS = true;
        } else if( !strcmp("-dev_id", argv[i]) && argc > (i+1) ) {
            dev_id = atoi(argv[++i]);
        } else if( !strcmp("-btmode", argv[i]) && argc > (i+1) ) {
            BTMode v = getBTMode(argv[++i]);
            if( BTMode::NONE != v ) {
                btMode = v;
            }
        } else if( !strcmp("-mac", argv[i]) && argc > (i+1) ) {
            std::string macstr = std::string(argv[++i]);
            waitForDevice = EUI48(macstr);
        } else if( !strcmp("-wl", argv[i]) && argc > (i+1) ) {
            std::string macstr = std::string(argv[++i]);
            std::shared_ptr<EUI48> wlmac( new EUI48(macstr) );
            fprintf(stderr, "Whitelist + %s\n", wlmac->toString().c_str());
            WHITELIST.push_back( wlmac );
            USE_WHITELIST = true;
        } else if( !strcmp("-disconnect", argv[i]) ) {
            KEEP_CONNECTED = false;
        } else if( !strcmp("-keepDevice", argv[i]) ) {
            REMOVE_DEVICE = false;
        } else if( !strcmp("-count", argv[i]) && argc > (i+1) ) {
            MULTI_MEASUREMENTS = atoi(argv[++i]);
        } else if( !strcmp("-single", argv[i]) ) {
            MULTI_MEASUREMENTS = -1;
        }
    }
    fprintf(stderr, "pid %d\n", getpid());

    fprintf(stderr, "Run with '[-dev_id <adapter-index>] [-btmode <BT-MODE>] [-mac <device_address>] [-disconnect] [-count <number>] [-single] (-wl <device_address>)* [-show_update_events]'\n");

    fprintf(stderr, "MULTI_MEASUREMENTS %d\n", MULTI_MEASUREMENTS);
    fprintf(stderr, "KEEP_CONNECTED %d\n", KEEP_CONNECTED);
    fprintf(stderr, "REMOVE_DEVICE %d\n", REMOVE_DEVICE);
    fprintf(stderr, "USE_WHITELIST %d\n", USE_WHITELIST);
    fprintf(stderr, "dev_id %d\n", dev_id);
    fprintf(stderr, "btmode %s\n", getBTModeString(btMode).c_str());
    fprintf(stderr, "waitForDevice: %s\n", waitForDevice.toString().c_str());

    // initialize manager with given default BTMode
    DBTManager::get(btMode);

    if( waitForEnter ) {
        fprintf(stderr, "Press ENTER to continue\n");
        getchar();
    }
    fprintf(stderr, "****** TEST start\n");
    test(dev_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    fprintf(stderr, "****** TEST end\n");
}
