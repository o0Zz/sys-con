#pragma once

#include "BaseController.h"
#include "drivers/Xbox360Controller.h"

#define XBOX360_MAX_INPUTS 4

// Every report from the wireless receiver is prefixed with a 4-byte receiver header;
// the controller payload starts after it.
#define XBOX360_WIRELESS_HEADER_SIZE 4

class Xbox360WirelessController : public BaseController
{
private:
    bool m_is_connected[XBOX360_MAX_INPUTS];

    ControllerResult SetLED(uint16_t input_idx, Xbox360LEDValue value);

    ControllerResult OnControllerConnect(uint16_t input_idx);
    ControllerResult OnControllerDisconnect(uint16_t input_idx);

public:
    Xbox360WirelessController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
    virtual ~Xbox360WirelessController() override;

    ControllerResult OpenInterfaces() override;
    void CloseInterfaces() override;

    virtual ControllerResult ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;

    bool Support(ControllerFeature feature) override;

    uint16_t GetInputCount() override;

    ControllerResult SetRumble(uint16_t input_idx, float amp_high, float amp_low) override;

    bool IsControllerConnected(uint16_t input_idx) override;
};