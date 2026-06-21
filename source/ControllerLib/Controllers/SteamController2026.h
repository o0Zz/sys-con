#pragma once

#include "BaseController.h"

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

class SteamController2026 : public BaseController
{
private:
    RawInputData m_rawInput;
    ControllerResult WriteAckModeReport(uint16_t input_idx, uint8_t sequence);

public:
    SteamController2026(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
    virtual ~SteamController2026() override;

    virtual ControllerResult Initialize() override;

    virtual ControllerResult ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;
};

// https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/steam/controller_structs.h

#define HID_FEATURE_REPORT_BYTES 64
typedef struct
{
    unsigned char type;
    unsigned char length;
} FeatureReportHeader;

typedef struct
{
    unsigned char settingNum;
    unsigned short settingValue;
} ControllerSetting;

typedef struct
{
    unsigned char attributeTag;
    uint32_t attributeValue;
} ControllerAttribute;

typedef struct
{
    ControllerSetting settings[(HID_FEATURE_REPORT_BYTES - sizeof(FeatureReportHeader)) / sizeof(ControllerSetting)];
} MsgSetSettingsValues, MsgGetSettingsValues, MsgGetSettingsDefaults, MsgGetSettingsMaxs;

typedef struct
{
    ControllerAttribute attributes[(HID_FEATURE_REPORT_BYTES - sizeof(FeatureReportHeader)) / sizeof(ControllerAttribute)];
} MsgGetAttributes;

typedef struct
{
    unsigned char attributeTag;
    char attributeValue[20];
} MsgGetStringAttribute;

typedef struct
{
    unsigned char mode;
} MsgSetControllerMode;

typedef struct
{
    unsigned char which_pad;
    unsigned short pulse_duration;
    unsigned short pulse_interval;
    unsigned short pulse_count;
    short dBgain;
    unsigned char priority;
} MsgFireHapticPulse;

typedef struct
{
    uint8_t mode;
} MsgHapticSetMode;

typedef struct
{
    uint8_t side;         // 0x01 = L, 0x02 = R, 0x03 = Both
    uint8_t cmd;          // 0 = Off, 1 = tick, 2 = click, 3 = tone, 4 = rumble, 5 =
                          // rumble_noise, 6 = script, 7 = sweep,
    uint8_t ui_intensity; // 0-4 (0 = default)
    int8_t dBgain;        // dB Can be positive (reasonable clipping / limiting will apply)
    uint16_t freq;        // Frequency of tone (if applicable)
    int16_t dur_ms;       // Duration of tone / rumble (if applicable) (neg = infinite)

    uint16_t noise_intensity;
    uint16_t lfo_freq;      // Drives both tone and rumble generators
    uint8_t lfo_depth;      // percentage, typically 100
    uint8_t rand_tone_gain; // Randomize each LFO cycle's gain
    uint8_t script_id;      // Used w/ dBgain for scripted haptics

    uint16_t lss_start_freq; // Used w/ Log Sine Sweep
    uint16_t lss_end_freq;   // Ditto
} MsgTriggerHaptic;

typedef struct
{
    uint8_t unRumbleType;
    uint16_t unIntensity;
    uint16_t unLeftMotorSpeed;
    uint16_t unRightMotorSpeed;
    int8_t nLeftGain;
    int8_t nRightGain;
} MsgSimpleRumbleCmd;

typedef struct
{
    FeatureReportHeader header;
    union
    {
        MsgSetSettingsValues setSettingsValues;
        MsgGetSettingsValues getSettingsValues;
        MsgGetSettingsMaxs getSettingsMaxs;
        MsgGetSettingsDefaults getSettingsDefaults;
        MsgGetAttributes getAttributes;
        MsgSetControllerMode controllerMode;
        MsgFireHapticPulse fireHapticPulse;
        MsgGetStringAttribute getStringAttribute;
        MsgHapticSetMode hapticMode;
        MsgTriggerHaptic triggerHaptic;
        MsgSimpleRumbleCmd simpleRumble;
    } payload;

} FeatureReportMsg;

// https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/steam/controller_constants.h

/*
  Simple DirectMedia Layer
  Copyright (C) 2021 Valve Corporation

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

extern "C"
{
    // This enum contains all of the messages exchanged between the host and the target (only add to this enum and never change the order)
    enum FeatureReportMessageIDs
    {
        ID_SET_DIGITAL_MAPPINGS = 0x80,
        ID_CLEAR_DIGITAL_MAPPINGS = 0x81,
        ID_GET_DIGITAL_MAPPINGS = 0x82,
        ID_GET_ATTRIBUTES_VALUES = 0x83,
        ID_GET_ATTRIBUTE_LABEL = 0x84,
        ID_SET_DEFAULT_DIGITAL_MAPPINGS = 0x85,
        ID_FACTORY_RESET = 0x86,
        ID_SET_SETTINGS_VALUES = 0x87,
        ID_CLEAR_SETTINGS_VALUES = 0x88,
        ID_GET_SETTINGS_VALUES = 0x89,
        ID_GET_SETTING_LABEL = 0x8A,
        ID_GET_SETTINGS_MAXS = 0x8B,
        ID_GET_SETTINGS_DEFAULTS = 0x8C,
        ID_SET_CONTROLLER_MODE = 0x8D,
        ID_LOAD_DEFAULT_SETTINGS = 0x8E,
        ID_TRIGGER_HAPTIC_PULSE = 0x8F,

        ID_TURN_OFF_CONTROLLER = 0x9F,

        ID_GET_DEVICE_INFO = 0xA1,

        ID_CALIBRATE_TRACKPADS = 0xA7,
        ID_RESERVED_0 = 0xA8,
        ID_SET_SERIAL_NUMBER = 0xA9,
        ID_GET_TRACKPAD_CALIBRATION = 0xAA,
        ID_GET_TRACKPAD_FACTORY_CALIBRATION = 0xAB,
        ID_GET_TRACKPAD_RAW_DATA = 0xAC,
        ID_ENABLE_PAIRING = 0xAD,
        ID_GET_STRING_ATTRIBUTE = 0xAE,
        ID_RADIO_ERASE_RECORDS = 0xAF,
        ID_RADIO_WRITE_RECORD = 0xB0,
        ID_SET_DONGLE_SETTING = 0xB1,
        ID_DONGLE_DISCONNECT_DEVICE = 0xB2,
        ID_DONGLE_COMMIT_DEVICE = 0xB3,
        ID_DONGLE_GET_WIRELESS_STATE = 0xB4,
        ID_CALIBRATE_GYRO = 0xB5,
        ID_PLAY_AUDIO = 0xB6,
        ID_AUDIO_UPDATE_START = 0xB7,
        ID_AUDIO_UPDATE_DATA = 0xB8,
        ID_AUDIO_UPDATE_COMPLETE = 0xB9,
        ID_GET_CHIPID = 0xBA,

        ID_CALIBRATE_JOYSTICK = 0xBF,
        ID_CALIBRATE_ANALOG_TRIGGERS = 0xC0,
        ID_SET_AUDIO_MAPPING = 0xC1,
        ID_CHECK_GYRO_FW_LOAD = 0xC2,
        ID_CALIBRATE_ANALOG = 0xC3,
        ID_DONGLE_GET_CONNECTED_SLOTS = 0xC4,

        ID_RESET_IMU = 0xCE,

        // Deck only
        ID_TRIGGER_HAPTIC_CMD = 0xEA,
        ID_TRIGGER_RUMBLE_CMD = 0xEB,
    };

    typedef enum
    {
        LIZARD_MODE_OFF,
        LIZARD_MODE_ON,
    } LizardModeState_t;

    // Read-write controller settings (only add to this enum and never change the order)
    typedef enum
    {
        SETTING_MOUSE_SENSITIVITY,
        SETTING_MOUSE_ACCELERATION,
        SETTING_TRACKBALL_ROTATION_ANGLE,
        SETTING_HAPTIC_INTENSITY_UNUSED,
        SETTING_LEFT_GAMEPAD_STICK_ENABLED,
        SETTING_RIGHT_GAMEPAD_STICK_ENABLED,
        SETTING_USB_DEBUG_MODE,
        SETTING_LEFT_TRACKPAD_MODE,
        SETTING_RIGHT_TRACKPAD_MODE,
        SETTING_LIZARD_MODE,

        // 10
        SETTING_DPAD_DEADZONE,
        SETTING_MINIMUM_MOMENTUM_VEL,
        SETTING_MOMENTUM_DECAY_AMOUNT,
        SETTING_TRACKPAD_RELATIVE_MODE_TICKS_PER_PIXEL,
        SETTING_HAPTIC_INCREMENT,
        SETTING_DPAD_ANGLE_SIN,
        SETTING_DPAD_ANGLE_COS,
        SETTING_MOMENTUM_VERTICAL_DIVISOR,
        SETTING_MOMENTUM_MAXIMUM_VELOCITY,
        SETTING_TRACKPAD_Z_ON,

        // 20
        SETTING_TRACKPAD_Z_OFF,
        SETTING_SENSITIVITY_SCALE_AMOUNT,
        SETTING_LEFT_TRACKPAD_SECONDARY_MODE,
        SETTING_RIGHT_TRACKPAD_SECONDARY_MODE,
        SETTING_SMOOTH_ABSOLUTE_MOUSE,
        SETTING_STEAMBUTTON_POWEROFF_TIME,
        SETTING_UNUSED_1,
        SETTING_TRACKPAD_OUTER_RADIUS,
        SETTING_TRACKPAD_Z_ON_LEFT,
        SETTING_TRACKPAD_Z_OFF_LEFT,

        // 30
        SETTING_TRACKPAD_OUTER_SPIN_VEL,
        SETTING_TRACKPAD_OUTER_SPIN_RADIUS,
        SETTING_TRACKPAD_OUTER_SPIN_HORIZONTAL_ONLY,
        SETTING_TRACKPAD_RELATIVE_MODE_DEADZONE,
        SETTING_TRACKPAD_RELATIVE_MODE_MAX_VEL,
        SETTING_TRACKPAD_RELATIVE_MODE_INVERT_Y,
        SETTING_TRACKPAD_DOUBLE_TAP_BEEP_ENABLED,
        SETTING_TRACKPAD_DOUBLE_TAP_BEEP_PERIOD,
        SETTING_TRACKPAD_DOUBLE_TAP_BEEP_COUNT,
        SETTING_TRACKPAD_OUTER_RADIUS_RELEASE_ON_TRANSITION,

        // 40
        SETTING_RADIAL_MODE_ANGLE,
        SETTING_HAPTIC_INTENSITY_MOUSE_MODE,
        SETTING_LEFT_DPAD_REQUIRES_CLICK,
        SETTING_RIGHT_DPAD_REQUIRES_CLICK,
        SETTING_LED_BASELINE_BRIGHTNESS,
        SETTING_LED_USER_BRIGHTNESS,
        SETTING_ENABLE_RAW_JOYSTICK,
        SETTING_ENABLE_FAST_SCAN,
        SETTING_IMU_MODE,
        SETTING_WIRELESS_PACKET_VERSION,

        // 50
        SETTING_SLEEP_INACTIVITY_TIMEOUT,
        SETTING_TRACKPAD_NOISE_THRESHOLD,
        SETTING_LEFT_TRACKPAD_CLICK_PRESSURE,
        SETTING_RIGHT_TRACKPAD_CLICK_PRESSURE,
        SETTING_LEFT_BUMPER_CLICK_PRESSURE,
        SETTING_RIGHT_BUMPER_CLICK_PRESSURE,
        SETTING_LEFT_GRIP_CLICK_PRESSURE,
        SETTING_RIGHT_GRIP_CLICK_PRESSURE,
        SETTING_LEFT_GRIP2_CLICK_PRESSURE,
        SETTING_RIGHT_GRIP2_CLICK_PRESSURE,

        // 60
        SETTING_PRESSURE_MODE,
        SETTING_CONTROLLER_TEST_MODE,
        SETTING_TRIGGER_MODE,
        SETTING_TRACKPAD_Z_THRESHOLD,
        SETTING_FRAME_RATE,
        SETTING_TRACKPAD_FILT_CTRL,
        SETTING_TRACKPAD_CLIP,
        SETTING_DEBUG_OUTPUT_SELECT,
        SETTING_TRIGGER_THRESHOLD_PERCENT,
        SETTING_TRACKPAD_FREQUENCY_HOPPING,

        // 70
        SETTING_HAPTICS_ENABLED,
        SETTING_STEAM_WATCHDOG_ENABLE,
        SETTING_TIMP_TOUCH_THRESHOLD_ON,
        SETTING_TIMP_TOUCH_THRESHOLD_OFF,
        SETTING_FREQ_HOPPING,
        SETTING_TEST_CONTROL,
        SETTING_HAPTIC_MASTER_GAIN_DB,
        SETTING_THUMB_TOUCH_THRESH,
        SETTING_DEVICE_POWER_STATUS,
        SETTING_HAPTIC_INTENSITY,

        // 80
        SETTING_STABILIZER_ENABLED,
        SETTING_TIMP_MODE_MTE,
        SETTING_COUNT,

        // This is a special setting value use for callbacks and should not be set/get explicitly.
        SETTING_ALL = 0xFF
    } ControllerSettings;
}