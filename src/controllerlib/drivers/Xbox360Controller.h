#pragma once

#include "BaseController.h"

namespace controllerlib
{
    // References used:
    // https://github.com/paroj/xpad/blob/master/xpad.c

    _PACKED(struct Xbox360ButtonData {
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

        bool button5 : 1;
        bool button6 : 1;
        bool button11 : 1;
        bool dummy1 : 1;

        bool button1 : 1;
        bool button2 : 1;
        bool button3 : 1;
        bool button4 : 1;

        uint8_t Rx;
        uint8_t Ry;

        int16_t X;
        int16_t Y;
        int16_t Z;
        int16_t Rz;
    });

    enum Xbox360InputPacketType : uint8_t
    {
        XBOX360INPUT_BUTTON = 0,
        XBOX360INPUT_LED = 1,
        XBOX360INPUT_RUMBLE = 3,
    };

    enum Xbox360LEDValue : uint8_t
    {
        XBOX360LED_OFF = 0,
        XBOX360LED_ALLBLINK,
        XBOX360LED_TOPLEFTBLINK,
        XBOX360LED_TOPRIGHTBLINK,
        XBOX360LED_BOTTOMLEFTBLINK,
        XBOX360LED_BOTTOMRIGHTBLINK,
        XBOX360LED_TOPLEFT,
        XBOX360LED_TOPRIGHT,
        XBOX360LED_BOTTOMLEFT,
        XBOX360LED_BOTTOMRIGHT,
    };

    class Xbox360Controller : public BaseController
    {
    private:
        Status SetLED(uint16_t input_idx, Xbox360LEDValue value);

    public:
        // The wireless receiver wraps the same report in a 4-byte header; both drivers decode it here.
        static void DecodeReport(const Xbox360ButtonData *buttonData, RawInputData *rawData);

        Xbox360Controller(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
        virtual ~Xbox360Controller() override;

        Status Initialize() override;

        virtual Status ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;

        bool Support(ControllerFeature feature) const override { return feature == SUPPORTS_RUMBLE; }

        Status SetRumble(uint16_t input_idx, const RumbleValue &rumble) override;
    };
} // namespace controllerlib
