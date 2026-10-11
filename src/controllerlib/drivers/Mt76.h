/*
 * MT76 (MT7612U) radio of the Xbox Wireless Adapter.
 *
 * Ported from xow, Copyright (C) 2019 Medusalix, GPL-2.0-or-later
 * (https://github.com/medusalix/xow). Register definitions derive from the Linux mt76
 * driver, Copyright (C) Felix Fietkau, Lorenzo Bianconi and others, ISC license.
 */
#pragma once

#include "IUSBDevice.h"
#include "ILogger.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace controllerlib
{
    class IFirmwareSource
    {
    public:
        virtual ~IFirmwareSource() = default;

        // A short read means the end of the image.
        virtual size_t Read(uint8_t *buffer, size_t size) = 0;
    };

    using MacAddress = std::array<uint8_t, 6>;

    struct RadioEvent
    {
        enum class Type
        {
            None,
            PairButtonPressed,
            ClientAssociating,
            ClientPairing,
            ClientLost,
            ClientData,
        };

        Type type = Type::None;
        MacAddress address{};
        uint8_t wcid = 0;
        const uint8_t *data = nullptr;
        size_t size = 0;
    };

    class Mt76
    {
    public:
        enum class Led : uint32_t
        {
            Blink = 0,
            On = 1,
            Off = 2,
        };

        static constexpr uint8_t WcidCount = 16;
        static constexpr uint8_t EndpointRead = 0x85;
        static constexpr uint8_t EndpointReadPacket = 0x84;
        static constexpr uint8_t EndpointWrite = 0x04;
        // The platform posts bulk transfers from a 512-byte buffer.
        static constexpr size_t MaxTransferSize = 512;

        Mt76(IUSBInterface &control, IUSBEndpoint &out, ILogger &logger);

        Status Start(IFirmwareSource &firmware);
        void Stop();

        Status AssociateClient(uint8_t wcid, const MacAddress &address);
        Status RemoveClient(uint8_t wcid);
        Status PairClient(const MacAddress &address);
        Status SendClientPacket(uint8_t wcid, const MacAddress &address, const uint8_t *data, size_t size);

        Status SetPairing(bool enable);
        Status SetLed(Led mode);

        // The returned event points into transfer; it is valid only while transfer is.
        RadioEvent ParseTransfer(const uint8_t *transfer, size_t size) const;

        const MacAddress &GetAddress() const { return m_address; }

    private:
        Status LoadFirmware(IFirmwareSource &firmware);
        Status ResetLoadedFirmware();
        Status UploadFirmwareSegment(IFirmwareSource &firmware, uint32_t offset, uint32_t length);
        Status InitRegisters();
        Status CalibrateCrystal();
        Status InitChannels();
        Status WriteBeacon(bool pairing);

        Status SelectFunction(uint32_t function, uint32_t value);
        Status SetPowerMode(uint32_t mode);
        Status LoadCr(uint32_t mode);
        Status BurstWrite(uint32_t index, const uint8_t *values, size_t size);
        Status Calibrate(uint32_t calibration, uint32_t value);
        Status ConfigureChannel(uint8_t channel, uint8_t bandwidth, bool scan);
        uint8_t GetChannelPower(uint8_t channel);
        Status SendFirmwareCommand(uint32_t command, const uint8_t *data, size_t size);
        Status SendCommand(uint8_t command, const uint8_t *data, size_t size);
        Status SendWlanPacket(const uint8_t *data, size_t size);
        Status BulkWrite(const uint8_t *data, size_t size);

        Status EfuseRead(uint8_t address, uint8_t *out, uint8_t length);
        Status ReadRegister(uint16_t address, uint32_t *value, uint8_t request);
        Status WriteRegister(uint16_t address, uint32_t value, uint8_t request);
        Status ReadRegister(uint16_t address, uint32_t *value);
        Status WriteRegister(uint16_t address, uint32_t value);
        template <typename Condition>
        Status PollUntil(Condition condition);

        IUSBInterface &m_control;
        IUSBEndpoint &m_out;
        ILogger &m_logger;
        MacAddress m_address{};
    };
} // namespace controllerlib
