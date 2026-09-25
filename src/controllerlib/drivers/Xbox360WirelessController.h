#pragma once

#include "BaseController.h"
#include "drivers/Xbox360Controller.h"

#define XBOX360_MAX_INPUTS 4

namespace controllerlib
{
// Every report from the wireless receiver is prefixed with a 4-byte receiver header;
// the controller payload starts after it.
#define XBOX360_WIRELESS_HEADER_SIZE 4

    class Xbox360WirelessController : public BaseController
    {
    private:
        bool m_is_connected[XBOX360_MAX_INPUTS];

        Status SetLED(uint16_t input_idx, Xbox360LEDValue value);

        Status OnControllerConnect(uint16_t input_idx);
        Status OnControllerDisconnect(uint16_t input_idx);

    public:
        Xbox360WirelessController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
        virtual ~Xbox360WirelessController() override;

        Status OpenInterfaces() override;
        void CloseInterfaces() override;

        virtual Status ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;

        bool Support(ControllerFeature feature) const override { return feature == SUPPORTS_RUMBLE; }

        uint16_t GetInputCount() override;

        Status SetRumble(uint16_t input_idx, const RumbleValue &rumble) override;

        bool IsControllerConnected(uint16_t input_idx) override;
    };
} // namespace controllerlib
