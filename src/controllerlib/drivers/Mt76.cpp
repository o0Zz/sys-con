/*
 * MT76 (MT7612U) radio of the Xbox Wireless Adapter.
 *
 * Ported from xow, Copyright (C) 2019 Medusalix, GPL-2.0-or-later
 * (https://github.com/medusalix/xow). Register definitions derive from the Linux mt76
 * driver, Copyright (C) Felix Fietkau, Lorenzo Bianconi and others, ISC license.
 */
#include "drivers/Mt76.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

namespace controllerlib
{
    namespace
    {
        constexpr uint32_t Bit(uint32_t n) { return 1u << n; }

        namespace reg
        {
            constexpr uint16_t AsicVersion = 0x0000;
            constexpr uint16_t CmbCtrl = 0x0020;
            constexpr uint16_t EfuseCtrl = 0x0024;
            constexpr uint32_t EfuseCtrlKick = Bit(30);
            constexpr uint16_t EfuseDataBase = 0x0028;
            constexpr uint16_t LdoCtrl1 = 0x0070;
            constexpr uint16_t XoCtrl5 = 0x0114;
            constexpr uint32_t XoCtrl5C2Val = 0x7f00;
            constexpr uint16_t XoCtrl6 = 0x0118;
            constexpr uint32_t XoCtrl6C2Ctrl = 0x7f00;
            constexpr uint16_t RfPatch = 0x0130;
            constexpr uint16_t WpdmaGloCfg = 0x0208;
            constexpr uint16_t WmmAifsn = 0x0214;
            constexpr uint16_t WmmCwmin = 0x0218;
            constexpr uint16_t WmmCwmax = 0x021c;
            constexpr uint16_t FceDmaAddr = 0x0230;
            constexpr uint16_t FceDmaLen = 0x0234;
            constexpr uint16_t UsbDmaCfg = 0x0238;
            constexpr uint16_t TsoCtrl = 0x0250;
            constexpr uint16_t PbfSysCtrl = 0x0400;
            constexpr uint16_t PbfCfg = 0x0404;
            constexpr uint16_t PbfTxMaxPcnt = 0x0408;
            constexpr uint16_t RfBypass0 = 0x0504;
            constexpr uint16_t RfSetting0 = 0x050c;
            constexpr uint16_t FcePseCtrl = 0x0800;
            constexpr uint16_t FceL2Stuff = 0x080c;
            constexpr uint16_t TxCpuFromFceBasePtr = 0x09a0;
            constexpr uint16_t TxCpuFromFceMaxCount = 0x09a4;
            constexpr uint16_t TxCpuFromFceCpuDescIdx = 0x09a8;
            constexpr uint16_t FcePdmaGlobalConf = 0x09c4;
            constexpr uint16_t PauseEnableControl1 = 0x0a38;
            constexpr uint16_t FceSkipFs = 0x0a6c;
            constexpr uint16_t MacCsr0 = 0x1000;
            constexpr uint16_t MacSysCtrl = 0x1004;
            constexpr uint32_t MacSysCtrlResetCsr = Bit(0);
            constexpr uint32_t MacSysCtrlResetBbp = Bit(1);
            constexpr uint32_t MacSysCtrlEnableTx = Bit(2);
            constexpr uint32_t MacSysCtrlEnableRx = Bit(3);
            constexpr uint16_t MacAddrDw0 = 0x1008;
            constexpr uint16_t MacBssidDw0 = 0x1010;
            constexpr uint16_t MaxLenCfg = 0x1018;
            constexpr uint16_t AmpduMaxLen20M1S = 0x1030;
            constexpr uint16_t AmpduMaxLen20M2S = 0x1034;
            constexpr uint16_t XifsTimeCfg = 0x1100;
            constexpr uint16_t BkoffSlotCfg = 0x1104;
            constexpr uint16_t ChTimeCfg = 0x110c;
            constexpr uint16_t BeaconTimeCfg = 0x1114;
            constexpr uint16_t PwrPinCfg = 0x1204;
            constexpr uint16_t RfPaModeAdj0 = 0x1228;
            constexpr uint16_t RfPaModeAdj1 = 0x122c;
            constexpr uint16_t DacclkEnDlyCfg = 0x1264;
            constexpr uint16_t EdcaCfgAc0 = 0x1300;
            constexpr uint16_t TxPinCfg = 0x1328;
            constexpr uint16_t TxSwCfg0 = 0x1330;
            constexpr uint16_t TxSwCfg1 = 0x1334;
            constexpr uint16_t TxopCtrlCfg = 0x1340;
            constexpr uint16_t TxRtsCfg = 0x1344;
            constexpr uint16_t TxTimeoutCfg = 0x1348;
            constexpr uint16_t TxRetryCfg = 0x134c;
            constexpr uint16_t CckProtCfg = 0x1364;
            constexpr uint16_t OfdmProtCfg = 0x1368;
            constexpr uint16_t Mm20ProtCfg = 0x136c;
            constexpr uint16_t Gf20ProtCfg = 0x1374;
            constexpr uint16_t Gf40ProtCfg = 0x1378;
            constexpr uint16_t ExpAckTime = 0x1380;
            constexpr uint16_t Tx0RfGainCorr = 0x13a0;
            constexpr uint16_t Tx1RfGainCorr = 0x13a4;
            constexpr uint16_t TxAlcCfg2 = 0x13a8;
            constexpr uint16_t TxAlcCfg3 = 0x13ac;
            constexpr uint16_t TxAlcCfg0 = 0x13b0;
            constexpr uint16_t TxAlcCfg4 = 0x13c0;
            constexpr uint16_t TxProtCfg6 = 0x13e0;
            constexpr uint16_t TxProtCfg7 = 0x13e4;
            constexpr uint16_t TxProtCfg8 = 0x13e8;
            constexpr uint16_t PifsTxCfg = 0x13ec;
            constexpr uint16_t RxFiltrCfg = 0x1400;
            constexpr uint16_t AutoRspCfg = 0x1404;
            constexpr uint16_t LegacyBasicRate = 0x1408;
            constexpr uint16_t HtBasicRate = 0x140c;
            constexpr uint16_t ExtCcaCfg = 0x141c;
            constexpr uint16_t PnPadMode = 0x150c;
            constexpr uint16_t TxopHldrEt = 0x1608;
            constexpr uint16_t WcidAddrBase = 0x1800;
            constexpr uint16_t BbpAgcBase = 0x2300;
            constexpr uint16_t UsbU3DmaCfg = 0x9018;
            constexpr uint16_t BeaconBase = 0xc000;

            constexpr uint16_t EdcaCfgAc(uint16_t n) { return EdcaCfgAc0 + (n << 2); }
            constexpr uint16_t BbpAgc(uint16_t n) { return BbpAgcBase + (n << 2); }
            constexpr uint16_t WcidAddr(uint8_t wcid) { return WcidAddrBase + wcid * 8; }
        } // namespace reg

        namespace eeprom
        {
            constexpr uint8_t ChipId = 0x000;
            constexpr uint8_t MacAddr = 0x004;
            constexpr uint8_t XtalTrim1 = 0x03a;
            constexpr uint8_t TxPower0Start2G = 0x056;
            constexpr uint8_t TxPower0Start5G = 0x062;
            constexpr uint8_t XtalTrim2 = 0x09e;
            constexpr uint8_t TxPowerGroupSize5G = 5;
        } // namespace eeprom

        enum VendorRequest : uint8_t
        {
            VendDevMode = 0x01,
            VendMultiWrite = 0x06,
            VendMultiRead = 0x07,
            VendWriteCfg = 0x46,
            VendReadCfg = 0x47,
        };

        enum McuCommand : uint8_t
        {
            CmdPacketTx = 0,
            CmdFunSetOp = 1,
            CmdLoadCr = 2,
            CmdInternalFwOp = 3,
            CmdBurstWrite = 8,
            CmdLedModeOp = 16,
            CmdPowerSavingOp = 20,
            CmdSwitchChannelOp = 30,
            CmdCalibrationOp = 31,
        };

        // Commands of the adapter's own firmware, sent through CmdInternalFwOp.
        enum FirmwareCommand : uint32_t
        {
            FwMacAddressSet = 0,
            FwClientAdd = 1,
            FwClientRemove = 2,
            FwChannelCandidatesSet = 7,
        };

        enum McuEvent : uint8_t
        {
            EvtButtonPress = 0x04,
            EvtPacketRx = 0x0c,
            EvtClientLost = 0x0e,
        };

        enum DmaPort : uint8_t
        {
            WlanPort = 0,
            CpuRxPort = 1,
            CpuTxPort = 2,
        };

        enum TxInfoType : uint8_t
        {
            NormalPacket = 0,
            CommandPacket = 1,
        };

        constexpr uint32_t FunctionQSelect = 1;
        constexpr uint32_t RadioOff = 0x30;
        constexpr uint32_t RadioOn = 0x31;
        constexpr uint32_t CrModeRfBbp = 2;
        constexpr uint32_t CalTempSensor = 2;
        constexpr uint32_t CalRxDcoc = 3;
        constexpr uint32_t CalRc = 4;
        constexpr uint8_t QselEdca = 2;
        constexpr uint8_t PhyTypeOfdm = 1;
        constexpr uint8_t BandwidthMhz20 = 0;
        constexpr uint8_t BandwidthMhz40 = 1;
        constexpr uint8_t BandwidthMhz80 = 2;

        constexpr uint8_t WlanManagement = 0x00;
        constexpr uint8_t WlanData = 0x02;
        constexpr uint8_t WlanAssociationRequest = 0x00;
        constexpr uint8_t WlanAssociationResponse = 0x01;
        constexpr uint8_t WlanReserved = 0x07;
        constexpr uint8_t WlanBeacon = 0x08;
        constexpr uint8_t WlanDisassociation = 0x0a;
        constexpr uint8_t WlanQosData = 0x08;
        constexpr uint8_t ReservedFramePairRequest = 0x01;

        constexpr uint32_t FwResetIvb = 0x01;
        constexpr uint32_t FwLoadIvb = 0x12;
        constexpr uint32_t McuIlmOffset = 0x80000;
        // Differs from the OpenWrt driver; this is where the adapter firmware expects it.
        constexpr uint32_t McuDlmOffset = 0x100000 + 0x10800;
        constexpr uint32_t DmaComplete = 0xc0000000;
        constexpr uint32_t RegisterOffset = 0x410000;
        constexpr uint8_t ChannelPowerMax = 0x2f;

        constexpr auto PollTimeout = std::chrono::seconds(1);

        // Every transfer carries a 4-byte info word and ends with a 4-byte zero marker.
        constexpr size_t TransferOverhead = 8;
        constexpr size_t FirmwareChunkSize = (Mt76::MaxTransferSize - TransferOverhead - 1) & ~size_t{3};

        _PACKED(struct FrameControl {
            uint16_t protocolVersion : 2;
            uint16_t type : 2;
            uint16_t subtype : 4;
            uint16_t toDs : 1;
            uint16_t fromDs : 1;
            uint16_t moreFragments : 1;
            uint16_t retry : 1;
            uint16_t powerManagement : 1;
            uint16_t moreData : 1;
            uint16_t protectedFrame : 1;
            uint16_t order : 1;
        });

        _PACKED(struct WlanFrame {
            FrameControl frameControl;
            uint16_t duration;
            uint8_t destination[6];
            uint8_t source[6];
            uint8_t bssId[6];
            uint16_t sequenceControl;
        });

        _PACKED(struct QosFrame {
            uint16_t qosControl;
        });

        _PACKED(struct AssociationResponseFrame {
            uint16_t capabilityInfo;
            uint16_t statusCode;
            uint16_t associationId;
            uint64_t unknown;
        });

        _PACKED(struct BeaconFrame {
            uint64_t timestamp;
            uint16_t interval;
            uint16_t capabilityInfo;
            uint16_t ssid;
        });

        _PACKED(struct RxWi {
            uint32_t dmaLength;

            uint32_t wcid : 8;
            uint32_t keyIndex : 2;
            uint32_t bssIndex : 3;
            uint32_t userDefined : 3;
            uint32_t mpduByteCount : 14;
            uint32_t reserved1 : 1;
            uint32_t eof : 1;

            uint32_t trafficId : 4;
            uint32_t sequenceNumber : 12;
            uint32_t mcs : 6;
            uint32_t ldpc : 1;
            uint32_t bandwidth : 2;
            uint32_t sgi : 1;
            uint32_t stbc : 1;
            uint32_t ldpcExSym : 1;
            uint32_t reserved2 : 1;
            uint32_t phyType : 3;

            uint8_t rssi[4];
            uint8_t bbpRxInfo[16];
        });

        _PACKED(struct TxWi {
            uint32_t fragment : 1;
            uint32_t mimoPowerSave : 1;
            uint32_t cfAck : 1;
            uint32_t timestamp : 1;
            uint32_t ampdu : 1;
            uint32_t mpduDensity : 3;
            uint32_t txop : 2;
            uint32_t ndpSoundingRate : 1;
            uint32_t rtsBwSig : 1;
            uint32_t ndpSoundingBw : 2;
            uint32_t sounding : 1;
            uint32_t lutEnable : 1;
            uint32_t mcs : 6;
            uint32_t ldpc : 1;
            uint32_t bandwidth : 2;
            uint32_t sgi : 1;
            uint32_t stbc : 1;
            uint32_t eTxBf : 1;
            uint32_t iTxBf : 1;
            uint32_t phyType : 3;

            uint32_t ack : 1;
            uint32_t nseq : 1;
            uint32_t baWindowSize : 6;
            uint32_t wcid : 8;
            uint32_t mpduByteCount : 14;
            uint32_t txbfPtSca : 1;
            uint32_t tim : 1;

            uint32_t iv;
            uint32_t eiv;

            uint32_t eapId : 8;
            uint32_t streamMode : 8;
            uint32_t powerAdjustment : 4;
            uint32_t reserved : 3;
            uint32_t groupId : 1;
            uint32_t packetId : 8;
        });

        _PACKED(struct RxInfoGeneric {
            uint32_t data : 25;
            uint32_t qsel : 2;
            uint32_t port : 3;
            uint32_t infoType : 2;
        });

        _PACKED(struct RxInfoCommand {
            uint32_t length : 14;
            uint32_t reserved : 1;
            uint32_t selfGen : 1;
            uint32_t sequence : 4;
            uint32_t eventType : 4;
            uint32_t pcieInterrupt : 1;
            uint32_t qsel : 2;
            uint32_t port : 3;
            uint32_t infoType : 2;
        });

        _PACKED(struct RxInfoPacket {
            uint32_t length : 14;
            uint32_t reserved : 2;
            uint32_t udpError : 1;
            uint32_t tcpError : 1;
            uint32_t ipError : 1;
            uint32_t is80211 : 1;
            uint32_t l3l4Done : 1;
            uint32_t macLength : 3;
            uint32_t pcieInterrupt : 1;
            uint32_t qsel : 2;
            uint32_t port : 3;
            uint32_t infoType : 2;
        });

        _PACKED(struct TxInfoCommand {
            uint32_t length : 16;
            uint32_t sequence : 4;
            uint32_t command : 7;
            uint32_t port : 3;
            uint32_t infoType : 2;
        });

        _PACKED(struct TxInfoPacket {
            uint32_t length : 16;
            uint32_t nextVld : 1;
            uint32_t txBurst : 1;
            uint32_t reserved1 : 1;
            uint32_t is80211 : 1;
            uint32_t tso : 1;
            uint32_t cso : 1;
            uint32_t reserved2 : 2;
            uint32_t wiv : 1;
            uint32_t qsel : 2;
            uint32_t port : 3;
            uint32_t infoType : 2;
        });

        _PACKED(struct BeaconTimeConfig {
            uint32_t interval : 16;
            uint32_t tsfTimerEnabled : 1;
            uint32_t tsfSyncMode : 2;
            uint32_t tbttTimerEnabled : 1;
            uint32_t transmitBeacon : 1;
            uint32_t reserved : 3;
            uint32_t tsfInsertionCompensation : 8;
        });

        _PACKED(struct ChannelConfig {
            uint8_t channel;
            uint8_t padding1;
            uint16_t padding2;
            uint16_t txRxSetting;
            uint16_t padding3;
            uint64_t padding4;
            uint8_t bandwidth;
            uint8_t txPower;
            uint8_t scan;
            uint8_t unknown;
        });

        _PACKED(struct EfuseControl {
            uint32_t addressOut : 6;
            uint32_t mode : 2;
            uint32_t ldoOffTime : 6;
            uint32_t ldoOnTime : 2;
            uint32_t addressIn : 10;
            uint32_t reserved : 4;
            uint32_t kick : 1;
            uint32_t select : 1;
        });

        _PACKED(struct FirmwareHeader {
            uint32_t ilmLength;
            uint32_t dlmLength;
            uint16_t buildVersion;
            uint16_t firmwareVersion;
            uint32_t padding;
            char buildTime[16];
        });

        _PACKED(struct DmaConfig {
            uint32_t rxBulkAggTimeout : 8;
            uint32_t rxBulkAggLimit : 8;
            uint32_t udmaTxWlDrop : 1;
            uint32_t wakeupEnabled : 1;
            uint32_t rxDropOrPad : 1;
            uint32_t txClear : 1;
            uint32_t txopHalt : 1;
            uint32_t rxBulkAggEnabled : 1;
            uint32_t rxBulkEnabled : 1;
            uint32_t txBulkEnabled : 1;
            uint32_t epOutValid : 6;
            uint32_t rxBusy : 1;
            uint32_t txBusy : 1;
        });

        static_assert(sizeof(WlanFrame) == 24);
        static_assert(sizeof(RxWi) == 32);
        static_assert(sizeof(TxWi) == 20);
        static_assert(sizeof(RxInfoGeneric) == 4 && sizeof(RxInfoCommand) == 4 && sizeof(RxInfoPacket) == 4);
        static_assert(sizeof(TxInfoCommand) == 4 && sizeof(TxInfoPacket) == 4);
        static_assert(sizeof(ChannelConfig) == 20);
        static_assert(sizeof(FirmwareHeader) == 32);

        template <typename T>
        uint32_t AsWord(const T &value)
        {
            static_assert(sizeof(T) == sizeof(uint32_t));
            uint32_t word;
            std::memcpy(&word, &value, sizeof(word));
            return word;
        }

        template <typename T>
        T FromWord(uint32_t word)
        {
            static_assert(sizeof(T) == sizeof(uint32_t));
            T value;
            std::memcpy(&value, &word, sizeof(value));
            return value;
        }

        size_t PaddingTo4(size_t size) { return (4 - size % 4) % 4; }

        class ByteWriter
        {
        public:
            template <typename T>
            void Append(const T &value)
            {
                Append(reinterpret_cast<const uint8_t *>(&value), sizeof(value));
            }

            void Append(const uint8_t *data, size_t size) { m_bytes.insert(m_bytes.end(), data, data + size); }
            void Append(const MacAddress &address) { Append(address.data(), address.size()); }
            void Pad(size_t count) { m_bytes.insert(m_bytes.end(), count, 0); }

            const uint8_t *Data() const { return m_bytes.data(); }
            size_t Size() const { return m_bytes.size(); }

        private:
            std::vector<uint8_t> m_bytes;
        };

        WlanFrame ManagementFrame(uint8_t subtype, const uint8_t *destination, const MacAddress &self)
        {
            WlanFrame frame{};
            frame.frameControl.type = WlanManagement;
            frame.frameControl.subtype = subtype;
            std::memcpy(frame.destination, destination, sizeof(frame.destination));
            std::memcpy(frame.source, self.data(), self.size());
            std::memcpy(frame.bssId, self.data(), self.size());
            return frame;
        }

        uint8_t GetChannelGroup(uint8_t channel)
        {
            enum : uint8_t
            {
                Japan,
                Unii1,
                Unii2,
                Unii2e1,
                Unii2e2,
                Unii3,
            };

            if (channel >= 184 && channel <= 196)
                return Japan;
            if (channel <= 48)
                return Unii1;
            if (channel <= 64)
                return Unii2;
            if (channel <= 114)
                return Unii2e1;
            if (channel <= 144)
                return Unii2e2;
            return Unii3;
        }

        uint8_t GetChannelSubgroup(uint8_t channel)
        {
            constexpr uint8_t Low2G = 1, Mid2G = 2, High2G = 3, Low5G = 1, High5G = 2;

            if (channel >= 192)
                return High5G;
            if (channel >= 184)
                return Low5G;
            if (channel < 6)
                return Low2G;
            if (channel < 11)
                return Mid2G;
            if (channel < 15)
                return High2G;
            if (channel < 44)
                return Low5G;
            if (channel < 52)
                return High5G;
            if (channel < 58)
                return Low5G;
            if (channel < 98)
                return High5G;
            if (channel < 106)
                return Low5G;
            if (channel < 116)
                return High5G;
            if (channel < 130)
                return Low5G;
            if (channel < 149)
                return High5G;
            if (channel < 157)
                return Low5G;
            return High5G;
        }
    } // namespace

    Mt76::Mt76(IUSBInterface &control, IUSBEndpoint &out, ILogger &logger)
        : m_control(control), m_out(out), m_logger(logger)
    {
    }

    Status Mt76::Start(IFirmwareSource &firmware)
    {
        Status result = LoadFirmware(firmware);
        if (result != Status::Success)
            return result;

        if ((result = SelectFunction(FunctionQSelect, 1)) != Status::Success ||
            (result = SetPowerMode(RadioOn)) != Status::Success ||
            (result = LoadCr(CrModeRfBbp)) != Status::Success)
        {
            m_logger.Log(LogLevel::Error, "Mt76: Failed to turn the radio on (%s)", ToString(result));
            return result;
        }

        if ((result = InitRegisters()) != Status::Success)
            return result;

        if ((result = SendFirmwareCommand(FwMacAddressSet, m_address.data(), m_address.size())) != Status::Success)
            return result;

        // Without this reset, associations are unreliable.
        if ((result = WriteRegister(reg::MacSysCtrl, 0)) != Status::Success ||
            (result = WriteRegister(reg::RfBypass0, 0)) != Status::Success ||
            (result = WriteRegister(reg::RfSetting0, 0)) != Status::Success)
            return result;

        if ((result = Calibrate(CalTempSensor, 0)) != Status::Success ||
            (result = Calibrate(CalRxDcoc, 1)) != Status::Success ||
            (result = Calibrate(CalRc, 0)) != Status::Success)
        {
            m_logger.Log(LogLevel::Error, "Mt76: Calibration failed (%s)", ToString(result));
            return result;
        }

        if ((result = WriteRegister(reg::MacSysCtrl, reg::MacSysCtrlEnableTx | reg::MacSysCtrlEnableRx)) != Status::Success)
            return result;

        if ((result = InitChannels()) != Status::Success)
            return result;

        if ((result = WriteBeacon(false)) != Status::Success)
            return result;

        m_logger.Log(LogLevel::Info, "Mt76: Radio ready, address %02x:%02x:%02x:%02x:%02x:%02x",
                     m_address[0], m_address[1], m_address[2], m_address[3], m_address[4], m_address[5]);
        return Status::Success;
    }

    void Mt76::Stop()
    {
        if (SetLed(Led::Off) != Status::Success)
            m_logger.Log(LogLevel::Error, "Mt76: Failed to turn the LED off");

        if (SetPowerMode(RadioOff) != Status::Success)
            m_logger.Log(LogLevel::Error, "Mt76: Failed to turn the radio off");
    }

    Status Mt76::AssociateClient(uint8_t wcid, const MacAddress &address)
    {
        TxWi txWi{};
        txWi.phyType = PhyTypeOfdm;
        txWi.ack = true;
        txWi.wcid = 0xff;
        txWi.mpduByteCount = sizeof(WlanFrame) + sizeof(AssociationResponseFrame);

        AssociationResponseFrame association{};
        association.statusCode = 0x0110;
        association.associationId = 0x0f00;

        ByteWriter packet;
        packet.Append(txWi);
        packet.Append(ManagementFrame(WlanAssociationResponse, address.data(), m_address));
        packet.Append(association);

        const uint8_t clientData[] = {static_cast<uint8_t>(wcid - 1), 0x00, 0x00, 0x00, 0x40, 0x1f, 0x00, 0x00};

        Status result;
        // WCID 0 is reserved for beacons, so client WCIDs start at 1.
        if ((result = BurstWrite(reg::WcidAddr(wcid), address.data(), address.size())) != Status::Success)
            return result;

        if ((result = SendFirmwareCommand(FwClientAdd, clientData, sizeof(clientData))) != Status::Success)
            return result;

        return SendWlanPacket(packet.Data(), packet.Size());
    }

    Status Mt76::RemoveClient(uint8_t wcid)
    {
        const uint8_t clientData[] = {static_cast<uint8_t>(wcid - 1), 0x00, 0x00, 0x00};
        const MacAddress emptyAddress{};

        Status result = SendFirmwareCommand(FwClientRemove, clientData, sizeof(clientData));
        if (result != Status::Success)
            return result;

        return BurstWrite(reg::WcidAddr(wcid), emptyAddress.data(), emptyAddress.size());
    }

    Status Mt76::PairClient(const MacAddress &address)
    {
        const uint8_t pairingData[] = {0x70, 0x02, 0x00, 0x45, 0x55, 0x01, 0x0f, 0x8f, 0xff, 0x87, 0x1f};

        TxWi txWi{};
        txWi.phyType = PhyTypeOfdm;
        txWi.ack = true;
        txWi.wcid = 0xff;
        txWi.mpduByteCount = sizeof(WlanFrame) + sizeof(pairingData);

        ByteWriter packet;
        packet.Append(txWi);
        packet.Append(ManagementFrame(WlanReserved, address.data(), m_address));
        packet.Append(pairingData, sizeof(pairingData));

        return SendWlanPacket(packet.Data(), packet.Size());
    }

    Status Mt76::SendClientPacket(uint8_t wcid, const MacAddress &address, const uint8_t *data, size_t size)
    {
        TxWi txWi{};
        txWi.phyType = PhyTypeOfdm;
        txWi.ack = true;
        txWi.mpduByteCount = sizeof(WlanFrame) + sizeof(QosFrame) + size;

        WlanFrame wlanFrame{};
        wlanFrame.frameControl.type = WlanData;
        wlanFrame.frameControl.subtype = WlanQosData;
        wlanFrame.frameControl.fromDs = true;
        // Microseconds needed to transmit the frame.
        wlanFrame.duration = 144;
        std::memcpy(wlanFrame.destination, address.data(), address.size());
        std::memcpy(wlanFrame.source, m_address.data(), m_address.size());
        std::memcpy(wlanFrame.bssId, m_address.data(), m_address.size());

        const uint32_t wcidWord = __builtin_bswap32(wcid - 1u);
        const size_t headerSize = sizeof(TxWi) + sizeof(WlanFrame) + sizeof(QosFrame);

        ByteWriter command;
        command.Append(wcidWord);
        command.Pad(sizeof(uint32_t));
        command.Append(txWi);
        command.Append(wlanFrame);
        command.Append(QosFrame{});
        command.Pad(PaddingTo4(headerSize));
        command.Append(data, size);
        command.Pad(PaddingTo4(size));

        return SendCommand(CmdPacketTx, command.Data(), command.Size());
    }

    Status Mt76::SetPairing(bool enable)
    {
        Status result = WriteBeacon(enable);
        if (result != Status::Success)
            return result;

        m_logger.Log(LogLevel::Info, "Mt76: Pairing %s", enable ? "enabled" : "disabled");
        return Status::Success;
    }

    Status Mt76::SetLed(Led mode)
    {
        const uint32_t value = static_cast<uint32_t>(mode);
        return SendCommand(CmdLedModeOp, reinterpret_cast<const uint8_t *>(&value), sizeof(value));
    }

    RadioEvent Mt76::ParseTransfer(const uint8_t *transfer, size_t size) const
    {
        RadioEvent event;

        // The last 4 bytes repeat the info word and mark the end of the transfer.
        if (size <= 2 * sizeof(uint32_t))
            return event;

        uint32_t infoWord;
        std::memcpy(&infoWord, transfer, sizeof(infoWord));
        const auto info = FromWord<RxInfoGeneric>(infoWord);

        const uint8_t *packet = transfer + sizeof(uint32_t);
        const size_t packetSize = size - 2 * sizeof(uint32_t);
        bool isWlanPacket = false;

        if (info.port == CpuRxPort)
        {
            const auto command = FromWord<RxInfoCommand>(infoWord);

            if (command.eventType == EvtButtonPress)
            {
                event.type = RadioEvent::Type::PairButtonPressed;
                return event;
            }

            if (command.eventType == EvtClientLost)
            {
                event.type = RadioEvent::Type::ClientLost;
                event.wcid = packet[0];
                return event;
            }

            isWlanPacket = command.eventType == EvtPacketRx;
        }
        else if (info.port == WlanPort)
        {
            isWlanPacket = FromWord<RxInfoPacket>(infoWord).is80211;
        }

        if (!isWlanPacket || packetSize <= sizeof(RxWi) + sizeof(WlanFrame))
            return event;

        RxWi rxWi;
        WlanFrame wlanFrame;
        std::memcpy(&rxWi, packet, sizeof(rxWi));
        std::memcpy(&wlanFrame, packet + sizeof(RxWi), sizeof(wlanFrame));

        if (std::memcmp(wlanFrame.destination, m_address.data(), m_address.size()) != 0)
            return event;

        const uint8_t *body = packet + sizeof(RxWi) + sizeof(WlanFrame);
        const size_t bodySize = packetSize - sizeof(RxWi) - sizeof(WlanFrame);
        std::memcpy(event.address.data(), wlanFrame.source, event.address.size());
        event.wcid = rxWi.wcid;

        if (wlanFrame.frameControl.type == WlanManagement)
        {
            switch (wlanFrame.frameControl.subtype)
            {
                case WlanAssociationRequest:
                    event.type = RadioEvent::Type::ClientAssociating;
                    break;

                // 1537 controllers associate, disassociate and associate again while pairing,
                // and those disassociations never raise EvtClientLost.
                case WlanDisassociation:
                    event.type = RadioEvent::Type::ClientLost;
                    break;

                case WlanReserved:
                    if (bodySize >= 2 && body[1] == ReservedFramePairRequest)
                        event.type = RadioEvent::Type::ClientPairing;
                    break;
            }
        }
        else if (wlanFrame.frameControl.type == WlanData && wlanFrame.frameControl.subtype == WlanQosData)
        {
            // The QoS control field is followed by 2 bytes of L2 padding.
            constexpr size_t QosHeaderSize = sizeof(QosFrame) + sizeof(uint16_t);
            if (bodySize > QosHeaderSize)
            {
                event.type = RadioEvent::Type::ClientData;
                event.data = body + QosHeaderSize;
                event.size = bodySize - QosHeaderSize;
            }
        }

        return event;
    }

    Status Mt76::LoadFirmware(IFirmwareSource &firmware)
    {
        FirmwareHeader header;
        if (firmware.Read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header))
        {
            m_logger.Log(LogLevel::Error, "Mt76: Firmware image is shorter than its header");
            return Status::UnexpectedData;
        }

        char buildTime[sizeof(header.buildTime) + 1] = {};
        std::memcpy(buildTime, header.buildTime, sizeof(header.buildTime));
        m_logger.Log(LogLevel::Info, "Mt76: Firmware ILM %u bytes, DLM %u bytes, version 0x%04x, build 0x%04x, built %s",
                     header.ilmLength, header.dlmLength, header.firmwareVersion, header.buildVersion, buildTime);

        uint32_t dmaAddress;
        Status result = ReadRegister(reg::FceDmaAddr, &dmaAddress, VendReadCfg);
        if (result != Status::Success)
            return result;

        if (dmaAddress != 0 && (result = ResetLoadedFirmware()) != Status::Success)
            return result;

        DmaConfig dmaConfig{};
        dmaConfig.rxBulkEnabled = true;
        dmaConfig.txBulkEnabled = true;

        if ((result = WriteRegister(reg::UsbU3DmaCfg, AsWord(dmaConfig), VendWriteCfg)) != Status::Success ||
            (result = WriteRegister(reg::FcePseCtrl, 0x01)) != Status::Success ||
            (result = WriteRegister(reg::TxCpuFromFceBasePtr, 0x400230)) != Status::Success ||
            (result = WriteRegister(reg::TxCpuFromFceMaxCount, 0x01)) != Status::Success ||
            (result = WriteRegister(reg::TxCpuFromFceCpuDescIdx, 0x01)) != Status::Success ||
            (result = WriteRegister(reg::FcePdmaGlobalConf, 0x44)) != Status::Success ||
            (result = WriteRegister(reg::FceSkipFs, 0x03)) != Status::Success)
            return result;

        if ((result = UploadFirmwareSegment(firmware, McuIlmOffset, header.ilmLength)) != Status::Success)
        {
            m_logger.Log(LogLevel::Error, "Mt76: Failed to upload the firmware ILM (%s)", ToString(result));
            return result;
        }

        if ((result = UploadFirmwareSegment(firmware, McuDlmOffset, header.dlmLength)) != Status::Success)
        {
            m_logger.Log(LogLevel::Error, "Mt76: Failed to upload the firmware DLM (%s)", ToString(result));
            return result;
        }

        if ((result = WriteRegister(reg::FceDmaAddr, 0, VendWriteCfg)) != Status::Success ||
            (result = WriteRegister(FwLoadIvb, 0, VendDevMode)) != Status::Success)
            return result;

        result = PollUntil([this](bool *done) {
            uint32_t value;
            Status status = ReadRegister(reg::FceDmaAddr, &value, VendReadCfg);
            *done = value == 0x01;
            return status;
        });

        if (result != Status::Success)
        {
            m_logger.Log(LogLevel::Error, "Mt76: Firmware did not start (%s)", ToString(result));
            return result;
        }

        m_logger.Log(LogLevel::Info, "Mt76: Firmware running");
        return Status::Success;
    }

    Status Mt76::ResetLoadedFirmware()
    {
        m_logger.Log(LogLevel::Info, "Mt76: Firmware already running, resetting it");

        uint32_t patch;
        Status result = ReadRegister(reg::RfPatch, &patch, VendReadCfg);
        if (result != Status::Success)
            return result;

        // A radio that was already initialized does not come back without clearing this bit.
        if ((result = WriteRegister(reg::RfPatch, patch & ~Bit(19), VendWriteCfg)) != Status::Success ||
            (result = WriteRegister(FwResetIvb, 0, VendDevMode)) != Status::Success)
            return result;

        result = PollUntil([this](bool *done) {
            uint32_t value;
            Status status = ReadRegister(reg::FceDmaAddr, &value, VendReadCfg);
            *done = value == 0x80000000;
            return status;
        });

        if (result != Status::Success)
            m_logger.Log(LogLevel::Error, "Mt76: Firmware reset timed out (%s)", ToString(result));

        return result;
    }

    Status Mt76::UploadFirmwareSegment(IFirmwareSource &firmware, uint32_t offset, uint32_t length)
    {
        uint8_t transfer[MaxTransferSize];

        for (uint32_t position = 0; position < length; position += FirmwareChunkSize)
        {
            const uint32_t chunkSize = std::min<uint32_t>(length - position, FirmwareChunkSize);
            const uint32_t alignedSize = chunkSize + PaddingTo4(chunkSize);

            TxInfoCommand info{};
            info.port = CpuTxPort;
            info.infoType = NormalPacket;
            info.length = chunkSize;

            std::memset(transfer, 0, sizeof(transfer));
            std::memcpy(transfer, &info, sizeof(info));

            if (firmware.Read(transfer + sizeof(info), chunkSize) != chunkSize)
            {
                m_logger.Log(LogLevel::Error, "Mt76: Firmware image ends early (%u of %u bytes at offset 0x%x)", position, length, offset);
                return Status::UnexpectedData;
            }

            Status result;
            if ((result = WriteRegister(reg::FceDmaAddr, offset + position, VendWriteCfg)) != Status::Success ||
                (result = WriteRegister(reg::FceDmaLen, alignedSize << 16, VendWriteCfg)) != Status::Success)
                return result;

            if ((result = BulkWrite(transfer, sizeof(info) + alignedSize + sizeof(uint32_t))) != Status::Success)
                return result;

            const uint32_t complete = (alignedSize << 16) | DmaComplete;
            result = PollUntil([this, complete](bool *done) {
                uint32_t value;
                Status status = ReadRegister(reg::FceDmaLen, &value, VendReadCfg);
                *done = value == complete;
                return status;
            });

            if (result != Status::Success)
            {
                m_logger.Log(LogLevel::Error, "Mt76: Firmware chunk at 0x%x not acknowledged (%s)", offset + position, ToString(result));
                return result;
            }
        }

        return Status::Success;
    }

    Status Mt76::InitRegisters()
    {
        static constexpr struct
        {
            uint16_t address;
            uint32_t value;
        } Registers[] = {
            {reg::MacSysCtrl, reg::MacSysCtrlResetCsr | reg::MacSysCtrlResetBbp},
            {reg::UsbDmaCfg, 0},
            {reg::MacSysCtrl, 0},
            {reg::PwrPinCfg, 0},
            {reg::LdoCtrl1, 0x6b006464},
            {reg::WpdmaGloCfg, 0x70},
            {reg::WmmAifsn, 0x2273},
            {reg::WmmCwmin, 0x2344},
            {reg::WmmCwmax, 0x34aa},
            {reg::FceDmaAddr, 0x041200},
            {reg::TsoCtrl, 0},
            {reg::PbfSysCtrl, 0x080c00},
            {reg::PbfTxMaxPcnt, 0x1fbf1f1f},
            {reg::FcePseCtrl, 0x01},
            {reg::MacSysCtrl, reg::MacSysCtrlEnableTx | reg::MacSysCtrlEnableRx},
            {reg::AutoRspCfg, 0x13},
            {reg::MaxLenCfg, 0x3e3fff},
            {reg::AmpduMaxLen20M1S, 0xfffc9855},
            {reg::AmpduMaxLen20M2S, 0xff},
            {reg::BkoffSlotCfg, 0x0109},
            {reg::PwrPinCfg, 0},
            {reg::EdcaCfgAc(0), 0x064320},
            {reg::EdcaCfgAc(1), 0x0a4700},
            {reg::EdcaCfgAc(2), 0x043238},
            {reg::EdcaCfgAc(3), 0x03212f},
            {reg::TxPinCfg, 0x150f0f},
            {reg::TxSwCfg0, 0x101001},
            {reg::TxSwCfg1, 0x010000},
            {reg::TxopCtrlCfg, 0x10583f},
            {reg::TxTimeoutCfg, 0x0a0f90},
            {reg::TxRetryCfg, 0x47d01f0f},
            {reg::CckProtCfg, 0x03f40003},
            {reg::OfdmProtCfg, 0x03f40003},
            {reg::Mm20ProtCfg, 0x01742004},
            {reg::Gf20ProtCfg, 0x01742004},
            {reg::Gf40ProtCfg, 0x03f42084},
            {reg::ExpAckTime, 0x2c00dc},
            {reg::TxAlcCfg2, 0x22160a00},
            {reg::TxAlcCfg3, 0x22160a76},
            {reg::TxAlcCfg0, 0x3f3f1818},
            {reg::TxAlcCfg4, 0x0606},
            {reg::PifsTxCfg, 0x060fff},
            {reg::RxFiltrCfg, 0x017f17},
            {reg::LegacyBasicRate, 0x017f},
            {reg::HtBasicRate, 0x8003},
            {reg::PnPadMode, 0x02},
            {reg::TxopHldrEt, 0x02},
            {reg::TxProtCfg6, 0xe3f42004},
            {reg::TxProtCfg7, 0xe3f42084},
            {reg::TxProtCfg8, 0xe3f42104},
            {reg::DacclkEnDlyCfg, 0},
            {reg::RfPaModeAdj0, 0xee000000},
            {reg::RfPaModeAdj1, 0xee000000},
            {reg::Tx0RfGainCorr, 0x0f3c3c3c},
            {reg::Tx1RfGainCorr, 0x0f3c3c3c},
            {reg::PbfCfg, 0x1efebcf5},
            {reg::PauseEnableControl1, 0x0a},
            {reg::RfBypass0, 0x7f000000},
            {reg::RfSetting0, 0x1a800000},
            {reg::XifsTimeCfg, 0x33a40e0a},
            {reg::FceL2Stuff, 0x03ff0223},
            {reg::TxRtsCfg, 0},
            {reg::BeaconTimeCfg, 0x0640},
            {reg::ExtCcaCfg, 0xf0e4},
            {reg::ChTimeCfg, 0x015f},
        };

        Status result;
        for (const auto &entry : Registers)
        {
            if ((result = WriteRegister(entry.address, entry.value)) != Status::Success)
                return result;
        }

        if ((result = CalibrateCrystal()) != Status::Success)
            return result;

        if ((result = WriteRegister(reg::BbpAgc(8), 0x18365efa)) != Status::Success ||
            (result = WriteRegister(reg::BbpAgc(9), 0x18365efa)) != Status::Success)
            return result;

        if ((result = EfuseRead(eeprom::MacAddr, m_address.data(), m_address.size())) != Status::Success)
            return result;

        // Some adapters ship with addresses starting 6c:5d:3a, but controllers only
        // connect to 62:45:bx:xx:xx:xx.
        if (m_address[0] != 0x62)
        {
            m_address[0] = 0x62;
            m_address[1] = 0x45;
            m_address[2] = 0xbd;
        }

        if ((result = BurstWrite(reg::MacAddrDw0, m_address.data(), m_address.size())) != Status::Success ||
            (result = BurstWrite(reg::MacBssidDw0, m_address.data(), m_address.size())) != Status::Success)
            return result;

        uint32_t asicVersion, macVersion;
        uint8_t chipId[4];
        if ((result = ReadRegister(reg::AsicVersion, &asicVersion)) != Status::Success ||
            (result = ReadRegister(reg::MacCsr0, &macVersion)) != Status::Success ||
            (result = EfuseRead(eeprom::ChipId, chipId, sizeof(chipId))) != Status::Success)
            return result;

        m_logger.Log(LogLevel::Info, "Mt76: ASIC 0x%04x, MAC 0x%04x, chip 0x%04x", asicVersion >> 16, macVersion >> 16, (chipId[1] << 8) | chipId[2]);
        return Status::Success;
    }

    Status Mt76::CalibrateCrystal()
    {
        uint8_t trim[4];
        Status result = EfuseRead(eeprom::XtalTrim2, trim, sizeof(trim));
        if (result != Status::Success)
            return result;

        uint16_t value = (trim[3] << 8) | trim[2];
        int8_t offset = value & 0x7f;

        if ((value & 0xff) == 0xff)
            offset = 0;
        else if (value & 0x80)
            offset = -offset;

        value >>= 8;

        if (value == 0x00 || value == 0xff)
        {
            if ((result = EfuseRead(eeprom::XtalTrim1, trim, sizeof(trim))) != Status::Success)
                return result;

            value = ((trim[3] << 8) | trim[2]) & 0xff;

            if (value == 0x00 || value == 0xff)
                value = 0x14;
        }

        value = (value & 0x7f) + offset;

        uint32_t control;
        if ((result = ReadRegister(reg::XoCtrl5, &control)) != Status::Success)
            return result;

        control &= ~reg::XoCtrl5C2Val;

        if ((result = WriteRegister(reg::XoCtrl5, control | (value << 8), VendWriteCfg)) != Status::Success ||
            (result = WriteRegister(reg::XoCtrl6, reg::XoCtrl6C2Ctrl, VendWriteCfg)) != Status::Success)
            return result;

        return WriteRegister(reg::CmbCtrl, 0x0091a7ff);
    }

    Status Mt76::InitChannels()
    {
        // The original driver raises the power for channels 0x24 - 0x30, sometimes past the 0x2f maximum.
        static constexpr struct
        {
            uint8_t channel;
            uint8_t bandwidth;
            bool scan;
        } Channels[] = {
            {0x01, BandwidthMhz20, true},
            {0x06, BandwidthMhz20, true},
            {0x0b, BandwidthMhz20, true},
            {0x24, BandwidthMhz40, true},
            {0x28, BandwidthMhz40, false},
            {0x2c, BandwidthMhz40, true},
            {0x30, BandwidthMhz40, false},
            {0x95, BandwidthMhz80, true},
            {0x99, BandwidthMhz80, false},
            {0x9d, BandwidthMhz80, true},
            {0xa1, BandwidthMhz80, false},
            {0xa5, BandwidthMhz80, false},
        };

        Status result;
        for (const auto &entry : Channels)
        {
            if ((result = ConfigureChannel(entry.channel, entry.bandwidth, entry.scan)) != Status::Success)
                return result;
        }

        static constexpr uint8_t Candidates[] = {0x01, 0xa5, 0x0b, 0x01, 0x06, 0x0b, 0x24, 0x28, 0x2c, 0x30, 0x95, 0x99, 0x9d, 0xa1};

        ByteWriter values;
        for (uint32_t channel : Candidates)
            values.Append(channel);

        return SendFirmwareCommand(FwChannelCandidatesSet, values.Data(), values.Size());
    }

    Status Mt76::WriteBeacon(bool pairing)
    {
        static constexpr uint8_t BroadcastAddress[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        // Vendor information element (0xdd, length 0x10); byte 8 announces pairing mode.
        const uint8_t informationElement[] = {
            0xdd, 0x10, 0x00, 0x50, 0xf2, 0x11, 0x01, 0x10,
            static_cast<uint8_t>(pairing), 0xa5, 0x30, 0x99,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

        TxWi txWi{};
        txWi.phyType = PhyTypeOfdm;
        txWi.timestamp = true;
        txWi.nseq = true;
        txWi.mpduByteCount = sizeof(WlanFrame) + sizeof(BeaconFrame) + sizeof(informationElement);

        BeaconFrame beacon{};
        beacon.interval = 0x64;
        beacon.capabilityInfo = 0xc631;

        ByteWriter frame;
        frame.Append(txWi);
        frame.Append(ManagementFrame(WlanBeacon, BroadcastAddress, m_address));
        frame.Append(beacon);
        frame.Append(informationElement, sizeof(informationElement));

        uint32_t configWord;
        Status result = ReadRegister(reg::BeaconTimeCfg, &configWord);
        if (result != Status::Success)
            return result;

        auto config = FromWord<BeaconTimeConfig>(configWord);
        config.tsfTimerEnabled = true;
        config.tbttTimerEnabled = true;
        config.tsfSyncMode = 3;
        config.transmitBeacon = true;

        if ((result = BurstWrite(reg::BeaconBase, frame.Data(), frame.Size())) != Status::Success ||
            (result = WriteRegister(reg::BeaconTimeCfg, AsWord(config))) != Status::Success)
            return result;

        return Calibrate(CalRxDcoc, 0);
    }

    Status Mt76::SelectFunction(uint32_t function, uint32_t value)
    {
        const uint32_t data[] = {function, value};
        return SendCommand(CmdFunSetOp, reinterpret_cast<const uint8_t *>(data), sizeof(data));
    }

    Status Mt76::SetPowerMode(uint32_t mode)
    {
        return SendCommand(CmdPowerSavingOp, reinterpret_cast<const uint8_t *>(&mode), sizeof(mode));
    }

    Status Mt76::LoadCr(uint32_t mode)
    {
        return SendCommand(CmdLoadCr, reinterpret_cast<const uint8_t *>(&mode), sizeof(mode));
    }

    Status Mt76::BurstWrite(uint32_t index, const uint8_t *values, size_t size)
    {
        ByteWriter data;
        data.Append(index + RegisterOffset);
        data.Append(values, size);
        return SendCommand(CmdBurstWrite, data.Data(), data.Size());
    }

    Status Mt76::Calibrate(uint32_t calibration, uint32_t value)
    {
        const uint32_t data[] = {calibration, value};
        return SendCommand(CmdCalibrationOp, reinterpret_cast<const uint8_t *>(data), sizeof(data));
    }

    Status Mt76::ConfigureChannel(uint8_t channel, uint8_t bandwidth, bool scan)
    {
        ChannelConfig config{};
        config.channel = channel;
        config.txRxSetting = 0x0101;
        config.bandwidth = bandwidth;
        config.txPower = GetChannelPower(channel);
        config.scan = scan;

        m_logger.Log(LogLevel::Debug, "Mt76: Channel %d, power %d", channel, config.txPower);
        return SendCommand(CmdSwitchChannelOp, reinterpret_cast<const uint8_t *>(&config), sizeof(config));
    }

    uint8_t Mt76::GetChannelPower(uint8_t channel)
    {
        const bool is24Ghz = channel <= 14;
        uint8_t tableAddress = is24Ghz ? eeprom::TxPower0Start2G : eeprom::TxPower0Start5G;

        if (!is24Ghz)
            tableAddress += GetChannelGroup(channel) * eeprom::TxPowerGroupSize5G;

        // Indexes are relative to the 16-byte efuse block, exactly as xow reads them.
        uint8_t entry[8];
        if (EfuseRead(tableAddress, entry, sizeof(entry)) != Status::Success)
        {
            m_logger.Log(LogLevel::Error, "Mt76: Failed to read the power table for channel %d", channel);
            return 0;
        }

        const uint8_t index = is24Ghz ? 4 : 5;
        const uint8_t target = entry[index];
        const uint8_t powerOffset = entry[index + GetChannelSubgroup(channel)];

        if ((powerOffset & Bit(7)) == 0)
            return target;

        const int8_t offset = powerOffset & 0x3f;
        const int power = (powerOffset & Bit(6)) ? target + offset : target - offset;

        if (power < 0)
            return 0;
        if (power > ChannelPowerMax)
            return ChannelPowerMax;
        return static_cast<uint8_t>(power);
    }

    Status Mt76::SendFirmwareCommand(uint32_t command, const uint8_t *data, size_t size)
    {
        ByteWriter payload;
        payload.Append(command);
        payload.Append(data, size);
        return SendCommand(CmdInternalFwOp, payload.Data(), payload.Size());
    }

    Status Mt76::SendCommand(uint8_t command, const uint8_t *data, size_t size)
    {
        const size_t padding = PaddingTo4(size);

        // Responses are not awaited, so the sequence number stays zero.
        TxInfoCommand info{};
        info.port = CpuTxPort;
        info.infoType = CommandPacket;
        info.command = command;
        info.length = size + padding;

        ByteWriter transfer;
        transfer.Append(info);
        transfer.Append(data, size);
        transfer.Pad(padding + sizeof(uint32_t));
        return BulkWrite(transfer.Data(), transfer.Size());
    }

    Status Mt76::SendWlanPacket(const uint8_t *data, size_t size)
    {
        const size_t padding = PaddingTo4(size);

        TxInfoPacket info{};
        info.port = WlanPort;
        info.infoType = NormalPacket;
        info.is80211 = true;
        info.wiv = true;
        info.qsel = QselEdca;
        info.length = size + padding;

        ByteWriter transfer;
        transfer.Append(info);
        transfer.Append(data, size);
        transfer.Pad(padding + sizeof(uint32_t));
        return BulkWrite(transfer.Data(), transfer.Size());
    }

    Status Mt76::BulkWrite(const uint8_t *data, size_t size)
    {
        if (size > MaxTransferSize)
        {
            m_logger.Log(LogLevel::Error, "Mt76: Transfer of %d bytes exceeds the %d-byte limit", static_cast<int>(size), static_cast<int>(MaxTransferSize));
            return Status::InvalidArgument;
        }

        Status result = m_out.Write(data, size);
        if (result != Status::Success)
            m_logger.Log(LogLevel::Error, "Mt76: Bulk write of %d bytes failed (%s)", static_cast<int>(size), ToString(result));

        return result;
    }

    Status Mt76::EfuseRead(uint8_t address, uint8_t *out, uint8_t length)
    {
        uint32_t controlWord;
        Status result = ReadRegister(reg::EfuseCtrl, &controlWord);
        if (result != Status::Success)
            return result;

        // Reads cover a whole block of 4 * 32 bits.
        auto control = FromWord<EfuseControl>(controlWord);
        control.mode = 0;
        control.addressIn = address & ~0x0f;
        control.kick = true;

        if ((result = WriteRegister(reg::EfuseCtrl, AsWord(control))) != Status::Success)
            return result;

        result = PollUntil([this](bool *done) {
            uint32_t value;
            Status status = ReadRegister(reg::EfuseCtrl, &value);
            *done = (value & reg::EfuseCtrlKick) == 0;
            return status;
        });

        if (result != Status::Success)
        {
            m_logger.Log(LogLevel::Error, "Mt76: Efuse read at 0x%03x timed out", address);
            return result;
        }

        for (uint8_t i = 0; i < length; i += sizeof(uint32_t))
        {
            uint32_t value;
            if ((result = ReadRegister(reg::EfuseDataBase + (address & 0x0c) + i, &value)) != Status::Success)
                return result;

            std::memcpy(out + i, &value, std::min<size_t>(length - i, sizeof(value)));
        }

        return Status::Success;
    }

    template <typename Condition>
    Status Mt76::PollUntil(Condition condition)
    {
        const auto start = std::chrono::steady_clock::now();

        for (;;)
        {
            bool done = false;
            Status result = condition(&done);
            if (result != Status::Success)
                return result;

            if (done)
                return Status::Success;

            if (std::chrono::steady_clock::now() - start > PollTimeout)
                return Status::Timeout;
        }
    }

    Status Mt76::ReadRegister(uint16_t address, uint32_t *value)
    {
        return ReadRegister(address, value, VendMultiRead);
    }

    Status Mt76::WriteRegister(uint16_t address, uint32_t value)
    {
        return WriteRegister(address, value, VendMultiWrite);
    }

    Status Mt76::ReadRegister(uint16_t address, uint32_t *value, uint8_t request)
    {
        constexpr uint8_t VendorDeviceIn = 0xc0;

        *value = 0;
        uint16_t length = sizeof(*value);
        Status result = m_control.ControlTransferInput(VendorDeviceIn, request, 0, address, value, &length);

        if (result == Status::Success && length != sizeof(*value))
            result = Status::UnexpectedData;

        if (result != Status::Success)
            m_logger.Log(LogLevel::Error, "Mt76: Reading register 0x%04x (request 0x%02x) failed (%s)", address, request, ToString(result));

        return result;
    }

    Status Mt76::WriteRegister(uint16_t address, uint32_t value, uint8_t request)
    {
        constexpr uint8_t VendorDeviceOut = 0x40;

        // Device mode requests carry the value in wValue and no data stage.
        Status result = request == VendDevMode
                            ? m_control.ControlTransferOutput(VendorDeviceOut, request, address, 0, nullptr, 0)
                            : m_control.ControlTransferOutput(VendorDeviceOut, request, 0, address, &value, sizeof(value));

        if (result != Status::Success)
            m_logger.Log(LogLevel::Error, "Mt76: Writing register 0x%04x (request 0x%02x) failed (%s)", address, request, ToString(result));

        return result;
    }
} // namespace controllerlib
