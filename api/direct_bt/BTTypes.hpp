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

#ifndef BT_TYPES_HPP_
#define BT_TYPES_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <vector>

#include "OctetTypes.hpp"
#include "BTAddress.hpp"
#include "UUID.hpp"

namespace direct_bt {

    enum AD_Type_Const : uint8_t {
        AD_FLAGS_LIMITED_MODE_BIT = 0x01,
        AD_FLAGS_GENERAL_MODE_BIT = 0x02
    };

    enum L2CAP_Channels : uint16_t {
        L2CAP_CID_SIGNALING     = 0x0001,
        L2CAP_CID_CONN_LESS     = 0x0002,
        L2CAP_CID_A2MP          = 0x0003,

        /* BT Core Spec v5.2:  Vol 3, Part G GATT: 5.2.2 LE channel requirements */
        L2CAP_CID_ATT           = 0x0004,

        L2CAP_CID_LE_SIGNALING  = 0x0005,
        L2CAP_CID_SMP           = 0x0006,
        L2CAP_CID_SMP_BREDR     = 0x0007,
        L2CAP_CID_DYN_START     = 0x0040,
        L2CAP_CID_DYN_END       = 0xffff,
        L2CAP_CID_LE_DYN_END    = 0x007f
    };

    /**
     * Protocol Service Multiplexers (PSM) Assigned numbers
     * <https://www.bluetooth.com/specifications/assigned-numbers/logical-link-control/>
     */
    enum L2CAP_PSM : uint16_t {
        L2CAP_PSM_UNDEF             = 0x0000,
        L2CAP_PSM_SDP               = 0x0001,
        L2CAP_PSM_RFCOMM            = 0x0003,
        L2CAP_PSM_TCSBIN            = 0x0005,
        L2CAP_PSM_TCSBIN_CORDLESS   = 0x0007,
        L2CAP_PSM_BNEP              = 0x000F,
        L2CAP_PSM_HID_CONTROL       = 0x0011,
        L2CAP_PSM_HID_INTERRUPT     = 0x0013,
        L2CAP_PSM_UPNP              = 0x0015,
        L2CAP_PSM_AVCTP             = 0x0017,
        L2CAP_PSM_AVDTP             = 0x0019,
        L2CAP_PSM_AVCTP_BROWSING    = 0x001B,
        L2CAP_PSM_UDI_C_PLANE       = 0x001D,
        L2CAP_PSM_ATT               = 0x001F,
        L2CAP_PSM_LE_DYN_START      = 0x0080,
        L2CAP_PSM_LE_DYN_END        = 0x00FF,
        L2CAP_PSM_DYN_START         = 0x1001,
        L2CAP_PSM_DYN_END           = 0xffff,
        L2CAP_PSM_AUTO_END          = 0x10ff
    };

    /**
     * BT Core Spec v5.2:  Vol 3, Part A L2CAP Spec: 6 State Machine
     */
    enum L2CAP_States : uint8_t {
        CLOSED,
        WAIT_CONNECTED,
        WAIT_CONNECTED_RSP,
        CONFIG,
        OPEN,
        WAIT_DISCONNECTED,
        WAIT_CREATE,
        WAIT_CONNECT,
        WAIT_CREATE_RSP,
        WAIT_MOVE,
        WAIT_MOVE_RSP,
        WAIT_MOVE_CONFIRM,
        WAIT_CONFIRM_RSP
    };

    /**
     * BT Core Spec v5.2:  Vol 3, Part A L2CAP Spec: 7.9 PRIORITIZING DATA OVER HCI
     *
     * In order for guaranteed channels to meet their guarantees,
     * L2CAP should prioritize traffic over the HCI transport in devices that support HCI.
     * Packets for Guaranteed channels should receive higher priority than packets for Best Effort channels.
     */

    /**
     * ​​Assigned numbers are used in Generic Access Profile (GAP) for inquiry response,
     * EIR data type values, manufacturer-specific data, advertising data,
     * low energy UUIDs and appearance characteristics, and class of device.
     * <p>
     * Type identifier values as defined in "Assigned Numbers - Generic Access Profile"
     * <https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/>
     * </p>
     * <p>
     * Also see Bluetooth Core Specification Supplement V9, Part A: 1, p 9 pp
     * for data format definitions.
     * </p>
     * <p>
     * For data segment layout see Bluetooth Core Specification V5.2 [Vol. 3, Part C, 11, p 1392]
     * </p>
     * <p>
     * https://www.bluetooth.com/specifications/archived-specifications/
     * </p>
     */
    enum GAP_T : uint8_t {
        // Last sync 2020-02-17 with <https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/>
        /** Flags */
        FLAGS                   = 0x01,
        /** Incomplete List of 16-bit Service Class UUID. (Supplement, Part A, section 1.1)*/
        UUID16_INCOMPLETE       = 0x02,
        /** Complete List of 16-bit Service Class UUID. (Supplement, Part A, section 1.1) */
        UUID16_COMPLETE         = 0x03,
        /** Incomplete List of 32-bit Service Class UUID. (Supplement, Part A, section 1.1) */
        UUID32_INCOMPLETE       = 0x04,
        /** Complete List of 32-bit Service Class UUID. (Supplement, Part A, section 1.1) */
        UUID32_COMPLETE         = 0x05,
        /** Incomplete List of 128-bit Service Class UUID. (Supplement, Part A, section 1.1) */
        UUID128_INCOMPLETE      = 0x06,
        /** Complete List of 128-bit Service Class UUID. (Supplement, Part A, section 1.1) */
        UUID128_COMPLETE        = 0x07,
        /** Shortened local name (Supplement, Part A, section 1.2) */
        NAME_LOCAL_SHORT        = 0x08,
        /** Complete local name (Supplement, Part A, section 1.2) */
        NAME_LOCAL_COMPLETE     = 0x09,
        /** Transmit power level (Supplement, Part A, section 1.5) */
        TX_POWER_LEVEL          = 0x0A,

        /**
         * SSP: Secure Simple Pairing Out of Band: Supplement, Part A, section 1.6
         * Supplement, Part A, Section 1.6: SSP OOB Data Block w/ SSP_OOB_LEN ([Vol 3] Part C, Section 5.2.2.7.)
         * <p>
         * SSP Class of device (Supplement, Part A, section 1.6).
         * </p>
         */
        SSP_CLASS_OF_DEVICE     = 0x0D,
        /** SSP: Simple Pairing Hash C and Simple Pairing Hash C-192 (Supplement, Part A 1.6) */
        SSP_HASH_C192           = 0x0E,
        /** SSP: Simple Pairing Randomizer R-192 (Supplement, Part A, section 1.6) */
        SSP_RANDOMIZER_R192     = 0x0F,

        /** Device ID Profile v 1.3 or later */
        DEVICE_ID               = 0x10,

        /** Security Manager TK Value (Supplement, Part A, section 1.8) */
        SEC_MGR_TK_VALUE        = 0x10,

        /** Security Manager Out of Band Flags (Supplement, Part A, section 1.7) */
        SEC_MGR_OOB_FLAGS       = 0x11,

        /** Slave Connection Interval Range */
        SLAVE_CONN_IVAL_RANGE   = 0x12,

        /** List of 16-bit Service Solicitation UUIDs (Supplement, Part A, section 1.10) */
        SOLICIT_UUID16          = 0x14,

        /** List of 128-bit Service Solicitation UUIDs (Supplement, Part A, section 1.10) */
        SOLICIT_UUID128         = 0x15,

        /** Service Data - 16-bit UUID (Supplement, Part A, section 1.11) */
        SVC_DATA_UUID16         = 0x16,

        /* Public Target Address (Supplement, Part A, section 1.13) */
        PUB_TRGT_ADDR           = 0x17,
        /* Random Target Address (Supplement, Part A, section 1.14) */
        RND_TRGT_ADDR           = 0x18,

        /** (GAP) Appearance (Supplement, Part A, section 1.12) */
        GAP_APPEARANCE          = 0x19,

        /** Advertising Interval (Supplement, Part A, section 1.15) */
        ADV_INTERVAL            = 0x1A,
        /** LE Bluetooth Device Address */
        LE_BT_DEV_ADDRESS       = 0x1B,
        /** LE ROLE */
        LE_ROLE                 = 0x1C,

        /** SSP: Simple Pairing Hash C-256 (Supplement, Part A 1.6) */
        SSP_HASH_C256           = 0x1D,
        /** SSP: Simple Pairing Randomizer R-256 (Supplement, Part A, section 1.6) */
        SSP_RANDOMIZER_R256     = 0x1E,

        /** List of 32-bit Service Solicitation UUID (Supplement, Part A, section 1.10) */
        SOLICIT_UUID32          = 0x1F,

        /** Service data, 32-bit UUID (Supplement, Part A, section 1.11) */
        SVC_DATA_UUID32         = 0x20,
        /** Service data, 128-bit UUID (Supplement, Part A, section 1.11) */
        SVC_DATA_UUID128        = 0x21,

        /** SSP: LE Secure Connections Confirmation Value (Supplement Part A, Section 1.6) */
        SSP_LE_SEC_CONN_ACK_VALUE   = 0x22,
        /** SSP: LE Secure Connections Random Value (Supplement Part A, Section 1.6) */
        SSP_LE_SEC_CONN_RND_VALUE   = 0x23,

        /* URI (Supplement, Part A, section 1.18) */
        URI                     = 0x24,

        /* Indoor Positioning - Indoor Positioning Service v1.0 or later */
        INDOOR_POSITIONING      = 0x25,

        /* Transport Discovery Data - Transport Discovery Service v1.0 or later */
        TX_DISCOVERY_DATA       = 0x26,

        /** LE Supported Features (Supplement, Part A, Section 1.19) */
        LE_SUPP_FEATURES        = 0x27,

        CH_MAP_UPDATE_IND       = 0x28,
        PB_ADV                  = 0x29,
        MESH_MESSAGE            = 0x2A,
        MESH_BEACON             = 0x2B,
        BIG_INFO                = 0x2C,
        BROADCAST_CODE          = 0x2D,
        INFO_DATA_3D            = 0x3D,

        /** Manufacturer id code and specific opaque data */
        MANUFACTURE_SPECIFIC    = 0xFF
    };


    // *************************************************
    // *************************************************
    // *************************************************

    class ManufactureSpecificData
    {
    public:
        uint16_t const company;
        std::string const companyName;
        POctets data;

        ManufactureSpecificData()
        : company(0), companyName(), data(0) {}

        ManufactureSpecificData(uint16_t const company, uint8_t const * const data, int const data_len);

        std::string getCompanyString() const;
        std::string toString() const;
    };

    // *************************************************
    // *************************************************
    // *************************************************

    /**
     * Collection of 'Advertising Data' (AD)
     * or 'Extended Inquiry Response' (EIR) information.
     */
    class EInfoReport
    {
    public:
        enum class Source : int {
            /** not available */
            NA,
            /* Advertising Data (AD) */
            AD,
            /** Extended Inquiry Response (EIR) */
            EIR
        };
        enum class Element : uint32_t {
            EVT_TYPE     = (1 << 0),
            BDADDR_TYPE  = (1 << 1),
            BDADDR       = (1 << 2),
            FLAGS        = (1 << 3),
            NAME         = (1 << 4),
            NAME_SHORT   = (1 << 5),
            RSSI         = (1 << 6),
            TX_POWER     = (1 << 7),
            MANUF_DATA   = (1 << 8),
            DEVICE_CLASS = (1 << 9),
            APPEARANCE   = (1 << 10),
            HASH         = (1 << 11),
            RANDOMIZER   = (1 << 12),
            DEVICE_ID    = (1 << 13)
        };

    private:
        Source source = Source::NA;
        uint64_t timestamp = 0;
        uint32_t data_set = 0;

        uint8_t evt_type = 0;
        BDAddressType addressType = BDAddressType::BDADDR_UNDEFINED;
        EUI48 address;

        uint8_t flags = 0;
        std::string name;
        std::string name_short;
        int8_t rssi = 0;
        int8_t tx_power = 0;
        std::shared_ptr<ManufactureSpecificData> msd = nullptr;
        std::vector<std::shared_ptr<uuid_t>> services;
        uint32_t device_class = 0;
        uint16_t appearance = 0;
        POctets hash;
        POctets randomizer;
        uint16_t did_source = 0;
        uint16_t did_vendor = 0;
        uint16_t did_product = 0;
        uint16_t did_version = 0;

        void set(Element bit) { data_set |= static_cast<uint32_t>(bit); }
        void setEvtType(uint8_t et) { evt_type = et; set(Element::EVT_TYPE); }
        void setFlags(uint8_t f) { flags = f; set(Element::FLAGS); }
        void setName(const uint8_t *buffer, int buffer_len);
        void setShortName(const uint8_t *buffer, int buffer_len);
        void setTxPower(int8_t v) { tx_power = v; set(Element::TX_POWER); }
        void setManufactureSpecificData(uint16_t const company, uint8_t const * const data, int const data_len) {
            msd = std::shared_ptr<ManufactureSpecificData>(new ManufactureSpecificData(company, data, data_len));
            set(Element::MANUF_DATA);
        }
        void addService(std::shared_ptr<uuid_t> const &uuid);
        void setDeviceClass(uint32_t c) { device_class= c; set(Element::DEVICE_CLASS); }
        void setAppearance(uint16_t a) { appearance= a; set(Element::APPEARANCE); }
        void setHash(const uint8_t * h) { hash.resize(16); memcpy(hash.get_wptr(), h, 16); set(Element::HASH); }
        void setRandomizer(const uint8_t * r) { randomizer.resize(16); memcpy(randomizer.get_wptr(), r, 16); set(Element::RANDOMIZER); }
        void setDeviceID(const uint16_t source, const uint16_t vendor, const uint16_t product, const uint16_t version) {
            did_source = source;
            did_vendor = vendor;
            did_product = product;
            did_version = version;
            set(Element::DEVICE_ID);
        }

        int next_data_elem(uint8_t *eir_elem_len, uint8_t *eir_elem_type, uint8_t const **eir_elem_data,
                           uint8_t const * data, int offset, int const size);

    public:
        EInfoReport() : hash(16, 0), randomizer(16, 0) {}

        static bool isSet(const uint32_t data_set, Element bit) { return 0 != (data_set & static_cast<uint32_t>(bit)); }
        static std::string dataSetToString(const uint32_t data_Set);

        void setSource(Source s) { source = s; }
        void setTimestamp(uint64_t ts) { timestamp = ts; }
        void setAddressType(BDAddressType at) { addressType = at; set(Element::BDADDR_TYPE); }
        void setAddress(EUI48 const &a) { address = a; set(Element::BDADDR); }
        void setRSSI(int8_t v) { rssi = v; set(Element::RSSI); }

        /**
         * Reads a complete Advertising Data (AD) Report
         * and returns the number of AD reports in form of a sharable list of EInfoReport;
         * <p>
         * See Bluetooth Core Specification V5.2 [Vol. 4, Part E, 7.7.65.2, p 2382]
         * <p>
         * https://www.bluetooth.com/specifications/archived-specifications/
         * </p>
         */
        static std::vector<std::shared_ptr<EInfoReport>> read_ad_reports(uint8_t const * data, uint8_t const data_length);

        /**
         * Reads the Extended Inquiry Response (EIR) or Advertising Data (AD) segments
         * and returns the number of parsed data segments;
         * <p>
         * AD as well as EIR information is passed in little endian order
         * in the same fashion data block:
         * <pre>
         * a -> {
         *             uint8_t len
         *             uint8_t type
         *             uint8_t data[len-1];
         *         }
         * b -> next block = a + 1 + len;
         * </pre>
         * </p>
         * <p>
         * See Bluetooth Core Specification V5.2 [Vol. 3, Part C, 11, p 1392]
         * and Bluetooth Core Specification Supplement V9, Part A: 1, p 9 + 2 Examples, p25..
         * and "Assigned Numbers - Generic Access Profile"
         * <https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/>
         * </p>
         * <p>
         * https://www.bluetooth.com/specifications/archived-specifications/
         * </p>
         */
        int read_data(uint8_t const * data, uint8_t const data_length);

        Source getSource() const { return source; }
        uint64_t getTimestamp() const { return timestamp; }
        bool isSet(Element bit) const { return 0 != (data_set & static_cast<uint32_t>(bit)); }
        uint32_t getDataSet() const { return data_set; }

        uint8_t getEvtType() const { return evt_type; }
        BDAddressType getAddressType() const { return addressType; }
        EUI48 const & getAddress() const { return address; }
        std::string const & getName() const { return name; }
        std::string const & getShortName() const { return name_short; }
        int8_t getRSSI() const { return rssi; }
        int8_t getTxPower() const { return tx_power; }

        std::shared_ptr<ManufactureSpecificData> getManufactureSpecificData() const { return msd; }
        std::vector<std::shared_ptr<uuid_t>> getServices() const { return services; }

        uint32_t getDeviceClass() const { return device_class; }
        uint16_t getAppearance() const { return appearance; }
        const TROOctets & getHash() const { return hash; }
        const TROOctets & getRandomizer() const { return randomizer; }
        uint16_t getDeviceIDSource() const { return did_source; }
        uint16_t getDeviceIDVendor() const { return did_vendor; }
        uint16_t getDeviceIDProduct() const { return did_product; }
        uint16_t getDeviceIDVersion() const { return did_version; }

        std::string getSourceString() const;
        std::string getAddressString() const { return address.toString(); }
        std::string dataSetToString() const;
        std::string toString() const;
    };

    // *************************************************
    // *************************************************
    // *************************************************

} // namespace direct_bt

#endif /* BT_TYPES_HPP_ */