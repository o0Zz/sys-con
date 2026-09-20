#pragma once

#include "BaseController.h"

namespace controllerlib
{
    // References used:
    // https://github.com/HandHeldLegend/SINPUT-LIB-HID
    // https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_sinput.c

#define SINPUT_INPUT_BUFFER_SIZE   64
#define SINPUT_COMMAND_BUFFER_SIZE 48

#define SINPUT_REPORT_ID_INPUT   0x01
#define SINPUT_REPORT_ID_REPLY   0x02
#define SINPUT_REPORT_ID_COMMAND 0x03

#define SINPUT_COMMAND_HAPTIC     0x01
#define SINPUT_HAPTIC_TYPE_RUMBLE 0x02

    _PACKED(struct SInputButtonData {
        uint8_t report_id;
        uint8_t plug_status;
        uint8_t charge_percent;

        uint8_t south : 1;
        uint8_t east : 1;
        uint8_t west : 1;
        uint8_t north : 1;
        uint8_t dpad_up : 1;
        uint8_t dpad_down : 1;
        uint8_t dpad_left : 1;
        uint8_t dpad_right : 1;

        uint8_t stick_left : 1;
        uint8_t stick_right : 1;
        uint8_t l_bumper : 1;
        uint8_t r_bumper : 1;
        uint8_t l_trigger : 1;
        uint8_t r_trigger : 1;
        uint8_t l_paddle_1 : 1;
        uint8_t r_paddle_1 : 1;

        uint8_t start : 1;
        uint8_t select : 1;
        uint8_t guide : 1;
        uint8_t share : 1;
        uint8_t l_paddle_2 : 1;
        uint8_t r_paddle_2 : 1;
        uint8_t l_touchpad : 1;
        uint8_t r_touchpad : 1;

        uint8_t power : 1;
        uint8_t misc_4 : 1;
        uint8_t misc_5 : 1;
        uint8_t misc_6 : 1;
        uint8_t misc_7 : 1;
        uint8_t misc_8 : 1;
        uint8_t misc_9 : 1;
        uint8_t misc_10 : 1;

        int16_t left_x;
        int16_t left_y;
        int16_t right_x;
        int16_t right_y;
        int16_t trigger_l;
        int16_t trigger_r;

        uint32_t imu_timestamp_us;
        int16_t accel_x;
        int16_t accel_y;
        int16_t accel_z;
        int16_t gyro_x;
        int16_t gyro_y;
        int16_t gyro_z;

        int16_t touchpad_1_x;
        int16_t touchpad_1_y;
        int16_t touchpad_1_pressure;
        int16_t touchpad_2_x;
        int16_t touchpad_2_y;
        int16_t touchpad_2_pressure;

        // 17 further reserved bytes follow, up to the 64 byte report.
    });

    static_assert(sizeof(SInputButtonData) == 47, "SInput input report layout must stay on the wire offsets SDL uses");

    class SInputController : public BaseController
    {
    public:
        SInputController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
        virtual ~SInputController() override;

        virtual Status ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;

        virtual bool Support(ControllerFeature feature) override;

        virtual Status SetRumble(uint16_t input_idx, float amp_high, float amp_low) override;

        virtual size_t GetMaxInputBufferSize() override;
    };
} // namespace controllerlib
