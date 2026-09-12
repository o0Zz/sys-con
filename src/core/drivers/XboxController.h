#pragma once

#include "BaseController.h"

// References used:
// https://github.com/felis/USB_Host_Shield_2.0/blob/master/XBOXOLD.cpp

_PACKED(struct XboxButtonData {
    uint8_t type;
    uint8_t length;

    bool dpad_up : 1;
    bool dpad_down : 1;
    bool dpad_left : 1;
    bool dpad_right : 1;

    bool button7 : 1;
    bool button8 : 1;
    bool button9 : 1;
    bool button10 : 1;

    uint8_t reserved;

    // These are analog
    uint8_t button1;
    uint8_t button2;
    uint8_t button3;
    uint8_t button4;

    uint8_t button5;
    uint8_t button6;

    uint8_t trigger_left;
    uint8_t trigger_right;

    int16_t stick_left_x;
    int16_t stick_left_y;
    int16_t stick_right_x;
    int16_t stick_right_y;
});

_PACKED(struct XboxRumbleData {
    uint8_t command;
    uint8_t size;
    uint8_t dummy1;
    uint8_t big;
    uint8_t dummy2;
    uint8_t little;
});

class XboxController : public BaseController
{
public:
    XboxController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
    virtual ~XboxController() override;

    virtual ControllerResult ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;

    bool Support(ControllerFeature feature) override;
    ControllerResult SetRumble(uint16_t input_idx, float amp_high, float amp_low) override;
};