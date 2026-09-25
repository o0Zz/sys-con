#pragma once

#include "BaseController.h"

namespace controllerlib
{
    // References used:
    // https://gist.github.com/ToadKing/30d65150410df13763f26f45abbd3700
    // https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/USB-HID-Notes.md
    // https://github.com/shinyquagsire23/HID-Joy-Con-Whispering/blob/master/hidtest/hidtest.cpp
    // https://github.com/torvalds/linux/blob/e08466a7c00733a501d3c5328d29ec974478d717/drivers/hid/hid-nintendo.c#L1723
    struct SwitchButtonData
    {
        uint8_t report_id;
        uint8_t timer;
        uint8_t bat_con;

        bool button1 : 1;
        bool button2 : 1;
        bool button3 : 1;
        bool button4 : 1;
        bool button5 : 1;
        bool button6 : 1;
        bool button7 : 1;
        bool button8 : 1;

        bool button9 : 1;
        bool button10 : 1;
        bool button11 : 1;
        bool button12 : 1;
        bool button13 : 1;
        bool button14 : 1;
        bool button15 : 1;
        bool dummy1 : 1;

        bool dpad_down : 1;
        bool dpad_up : 1;
        bool dpad_right : 1;
        bool dpad_left : 1;
        bool button16 : 1;
        bool button17 : 1;
        bool button18 : 1;
        bool button19 : 1;

        uint8_t stick_left[3];
        uint8_t stick_right[3];
    };

    _PACKED(struct SwitchIMUSample {
        int16_t accel_x;
        int16_t accel_y;
        int16_t accel_z;
        int16_t gyro_x;
        int16_t gyro_y;
        int16_t gyro_z;
    });

    // A 0x30 report carries three IMU samples 5 ms apart after the vibration byte, newest first.
    inline constexpr size_t SWITCH_IMU_OFFSET = 13;
    inline constexpr size_t SWITCH_IMU_SAMPLE_COUNT = 3;

    struct SwitchCalibration
    {
        uint16_t min;
        uint16_t max;
    };

    class SwitchController : public BaseController
    {
    private:
        SwitchCalibration cal_left_x;
        SwitchCalibration cal_left_y;
        SwitchCalibration cal_right_x;
        SwitchCalibration cal_right_y;
        uint8_t m_packet_counter = 0;

        static uint32_t AmplitudeStep(float amplitude);
        static void EncodeRumble(uint8_t *data, const RumbleActuator &actuator);

    public:
        SwitchController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
        virtual ~SwitchController() override;

        Status Initialize() override;

        virtual Status ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;

        size_t GetMaxInputBufferSize() override;

        bool Support(ControllerFeature feature) const override { return feature == SUPPORTS_RUMBLE || feature == SUPPORTS_MOTION; }

        Status SetRumble(uint16_t input_idx, const RumbleValue &rumble) override;
    };
} // namespace controllerlib
