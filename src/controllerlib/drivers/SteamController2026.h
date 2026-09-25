#pragma once

#include "BaseController.h"
#include <chrono>

namespace controllerlib
{
    // References used:
    // https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/steam/controller_structs.h
    // https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/steam/controller_constants.h

#define STEAMCONTROLLER_MAX_INPUTS 4

#define REPORT_INPUT                  0x42
#define REPORT_INPUT_BLE              0x45
#define REPORT_WIRELESS_STATUS_X      0x46
#define REPORT_WIRELESS_STATUS        0x79
#define HID_FEATURE_REPORT_BYTES      64
#define ID_SET_SETTINGS_VALUES        0x87
#define SETTING_LIZARD_MODE           0x09
#define LIZARD_MODE_OFF               0x00
#define SETTING_STEAM_WATCHDOG_ENABLE 0x47
#define WATCHDOG_DISABLE              0x00
#define ID_OUT_REPORT_HAPTIC_RUMBLE   0x80
#define HID_RUMBLE_OUTPUT_REPORT_BYTES 10

/* The pad stops the motors around 50 ms after the last haptic report it got, and the resend
   can only happen on a poll turn, so this has to leave room for a whole polling period. */
#define STEAMCONTROLLER_RUMBLE_RESEND_MS 25

    _PACKED(struct FeatureReportHeader {
        unsigned char type;
        unsigned char length;
    });

    _PACKED(struct ControllerSetting {
        unsigned char settingNum;
        unsigned short settingValue;
    });

    _PACKED(struct MsgSetSettingsValues {
        ControllerSetting settings[(HID_FEATURE_REPORT_BYTES - sizeof(FeatureReportHeader)) / sizeof(ControllerSetting)];
    });

    _PACKED(struct SetSettingsFeatureReportMsg {
        FeatureReportHeader header;
        MsgSetSettingsValues setSettingsValues;
    });

    _PACKED(struct MsgHapticRumbleSide {
        uint16_t speed;
        int8_t gain;
    });

    _PACKED(struct MsgHapticRumble {
        uint8_t type;
        uint16_t intensity;
        MsgHapticRumbleSide left;
        MsgHapticRumbleSide right;
    });

    _PACKED(struct HapticRumbleOutputReport {
        uint8_t report_id;
        MsgHapticRumble hapticRumble;
    });

    _PACKED(struct TritonWirelessStatus {
        uint8_t report_id;
        uint8_t state;
    });

    _PACKED(struct Steam2026ButtonData {
        uint8_t a : 1;
        uint8_t b : 1;
        uint8_t x : 1;
        uint8_t y : 1;
        uint8_t quickaccess : 1;
        uint8_t rstick : 1;
        uint8_t menu : 1;
        uint8_t r4 : 1;

        uint8_t r5 : 1;
        uint8_t r1 : 1;
        uint8_t dpad_down : 1;
        uint8_t dpad_right : 1;
        uint8_t dpad_left : 1;
        uint8_t dpad_up : 1;
        uint8_t view : 1;
        uint8_t lstick : 1;

        uint8_t steam : 1;
        uint8_t l4 : 1;
        uint8_t l5 : 1;
        uint8_t l1 : 1;
        uint8_t rstick_touch : 1;
        uint8_t rpad_touch : 1;
        uint8_t rpad : 1;
        uint8_t r2 : 1;

        uint8_t lstick_touch : 1;
        uint8_t lpad_touch : 1;
        uint8_t lpad : 1;
        uint8_t l2 : 1;
        uint8_t rgrip : 1;
        uint8_t lgrip : 1;
        uint8_t unused : 2;
    });

    _PACKED(struct Steam2026IMU {
        uint32_t timestamp;
        short sAccelX;
        short sAccelY;
        short sAccelZ;

        short sGyroX;
        short sGyroY;
        short sGyroZ;
    });

    _PACKED(struct Steam2026InputReport {
        uint8_t report_id;
        uint8_t seq_num;

        Steam2026ButtonData buttons;

        short left_trigger;
        short right_trigger;

        short left_stick_x;
        short left_stick_y;
        short right_stick_x;
        short right_stick_y;

        short left_pad_x;
        short left_pad_y;
        unsigned short left_pad_pressure;
        short right_pad_x;
        short right_pad_y;
        unsigned short right_pad_pressure;

        Steam2026IMU imu;
    });

    struct SteamControllerInfo
    {
        bool m_is_connected;
    };

    struct SteamControllerRumble
    {
        uint16_t speed_low;
        uint16_t speed_high;
        std::chrono::steady_clock::time_point sent_at;
    };

    class SteamController2026 : public BaseController
    {
    private:
        RawInputData m_rawInput;
        SteamControllerInfo m_controllerInfo[STEAMCONTROLLER_MAX_INPUTS];
        SteamControllerRumble m_rumble[STEAMCONTROLLER_MAX_INPUTS]{};
        uint8_t m_controller_count;

        Status OnControllerConnect(uint16_t input_idx);
        Status OnControllerDisconnect(uint16_t input_idx);
        Status UpdateLizard(uint16_t input_idx);
        Status SendFeatureReport(uint16_t input_idx, const uint8_t *buffer, uint16_t size);
        Status SendRumble(uint16_t input_idx);

    public:
        SteamController2026(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
        virtual ~SteamController2026() override;

        virtual Status Initialize() override;
        virtual uint16_t GetInputCount() override;

        virtual Status ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;

        Status ReadInput(NormalizedButtonData *normalData, uint16_t *input_idx, uint32_t timeout_us) override;

        virtual bool IsControllerConnected(uint16_t input_idx) override;

        bool Support(ControllerFeature feature) const override { return feature == SUPPORTS_RUMBLE || feature == SUPPORTS_MOTION; }

        Status SetRumble(uint16_t input_idx, const RumbleValue &rumble) override;
    };
} // namespace controllerlib
