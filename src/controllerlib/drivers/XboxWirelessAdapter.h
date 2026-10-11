#pragma once

#include "BaseController.h"
#include "drivers/Mt76.h"
#include <array>
#include <memory>
#include <optional>

namespace controllerlib
{
    // Microsoft's Xbox Wireless Adapter for Windows: an MT7612U radio that speaks GIP to
    // Xbox One and Series controllers over a proprietary 802.11 link. One input per client.
    class XboxWirelessAdapter : public BaseController
    {
    public:
        XboxWirelessAdapter(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger, std::unique_ptr<IFirmwareSource> &&firmware);
        ~XboxWirelessAdapter() override;

        Status Initialize() override;
        void Exit() override;

        Status OpenInterfaces() override;

        uint16_t GetInputCount() override { return CONTROLLER_MAX_INPUTS; }

        bool Support(ControllerFeature feature) const override { return feature == SUPPORTS_RUMBLE; }

        Status SetRumble(uint16_t input_idx, const RumbleValue &rumble) override;

        bool IsControllerConnected(uint16_t input_idx) override;

    protected:
        Status ReadNextBuffer(uint8_t *buffer, size_t *size, uint16_t *input_idx, uint32_t timeout_us) override;
        Status ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;

    private:
        struct Client
        {
            bool associated = false;
            bool announced = false;
            MacAddress address{};
            uint8_t sequence = 0;
            RawInputData input{};
        };

        Status HandleTransfer(const uint8_t *transfer, size_t transferSize, uint8_t *buffer, size_t *size, uint16_t *input_idx);
        Status OnClientAssociating(const MacAddress &address, uint16_t *input_idx);
        Status OnClientLost(uint8_t wcid, uint16_t *input_idx);
        Status OnPairButtonPressed();
        Status OnPairRequest(const MacAddress &address);
        Status OnAnnounce(uint16_t slot);

        Status SendGip(uint16_t slot, uint8_t command, uint8_t options, const uint8_t *payload, uint8_t payloadSize);
        Status SendGipAck(uint16_t slot, const uint8_t *frame);
        uint8_t NextSequence(Client &client);

        std::optional<uint16_t> FindSlot(const MacAddress &address) const;
        std::optional<uint16_t> FindFreeSlot() const;
        bool HasClients() const;

        std::unique_ptr<IFirmwareSource> m_firmware;
        std::unique_ptr<Mt76> m_radio;
        IUSBEndpoint *m_eventEndpoint = nullptr;
        IUSBEndpoint *m_packetEndpoint = nullptr;
        IUSBEndpoint *m_writeEndpoint = nullptr;
        bool m_blockOnEvents = false;
        std::array<Client, CONTROLLER_MAX_INPUTS> m_clients;
    };
} // namespace controllerlib
