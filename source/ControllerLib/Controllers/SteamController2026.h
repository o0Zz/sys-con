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

    bool Support(ControllerFeature feature) override;

    ControllerResult SetRumble(uint16_t input_idx, float amp_high, float amp_low) override;
};

// https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/steam/controller_structs.h

/*
  Simple DirectMedia Layer
  Copyright (C) 2020 Valve Corporation

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
#ifndef _CONTROLLER_STRUCTS_
#define _CONTROLLER_STRUCTS_

#pragma pack(1)

#define HID_FEATURE_REPORT_BYTES 64

// Header for all host <==> target messages
typedef struct
{
	unsigned char type;
	unsigned char length;
} FeatureReportHeader;

// Generic controller settings structure
typedef struct
{
	unsigned char settingNum;
	unsigned short settingValue;
} ControllerSetting;

// Generic controller attribute structure
typedef struct
{
	unsigned char attributeTag;
	uint32_t attributeValue;
} ControllerAttribute;

// Generic controller settings structure
typedef struct
{
	ControllerSetting settings[ ( HID_FEATURE_REPORT_BYTES - sizeof( FeatureReportHeader ) ) / sizeof( ControllerSetting ) ];
} MsgSetSettingsValues, MsgGetSettingsValues, MsgGetSettingsDefaults, MsgGetSettingsMaxs;

// Generic controller settings structure
typedef struct
{
	ControllerAttribute attributes[ ( HID_FEATURE_REPORT_BYTES - sizeof( FeatureReportHeader ) ) / sizeof( ControllerAttribute ) ];
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

// Trigger a haptic pulse
typedef struct {
	unsigned char which_pad;
	unsigned short pulse_duration;
	unsigned short pulse_interval;
	unsigned short pulse_count;
	short dBgain;
	unsigned char priority;
} MsgFireHapticPulse;

typedef struct {
	uint8_t mode;
} MsgHapticSetMode;

typedef enum {
	HAPTIC_TYPE_OFF,
	HAPTIC_TYPE_TICK,
	HAPTIC_TYPE_CLICK,
	HAPTIC_TYPE_TONE,
	HAPTIC_TYPE_RUMBLE,
	HAPTIC_TYPE_NOISE,
	HAPTIC_TYPE_SCRIPT,
	HAPTIC_TYPE_LOG_SWEEP,
} haptic_type_t;

typedef enum {
	HAPTIC_INTENSITY_SYSTEM,
	HAPTIC_INTENSITY_SHORT,
	HAPTIC_INTENSITY_MEDIUM,
	HAPTIC_INTENSITY_LONG,
	HAPTIC_INTENSITY_INSANE,
} haptic_intensity_t;

typedef struct {
	uint8_t side; 				// 0x01 = L, 0x02 = R, 0x03 = Both
	uint8_t cmd; 				// 0 = Off, 1 = tick, 2 = click, 3 = tone, 4 = rumble, 5 =
								// rumble_noise, 6 = script, 7 = sweep,
	uint8_t ui_intensity; 		// 0-4 (0 = default)
	int8_t dBgain; 				// dB Can be positive (reasonable clipping / limiting will apply)
	uint16_t freq; 				// Frequency of tone (if applicable)
	int16_t dur_ms; 			// Duration of tone / rumble (if applicable) (neg = infinite)

	uint16_t noise_intensity;
	uint16_t lfo_freq; 			// Drives both tone and rumble generators
	uint8_t lfo_depth; 			// percentage, typically 100
	uint8_t rand_tone_gain; 	// Randomize each LFO cycle's gain
	uint8_t script_id; 			// Used w/ dBgain for scripted haptics

	uint16_t lss_start_freq;	// Used w/ Log Sine Sweep
	uint16_t lss_end_freq;		// Ditto
} MsgTriggerHaptic;

typedef struct {
	uint8_t unRumbleType;
	uint16_t unIntensity;
	uint16_t unLeftMotorSpeed;
	uint16_t unRightMotorSpeed;
	int8_t nLeftGain;
	int8_t nRightGain;
} MsgSimpleRumbleCmd;

// This is the only message struct that application code should use to interact with feature request messages. Any new
// messages should be added to the union. The structures defined here should correspond to the ones defined in
// ValveDeviceCore.cpp.
//
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

// Triton and derivatives utilize output reports for haptic commands. This is a
// snapshot from Nov 2024 -- things may change.

// Triton Output Report Lengths #defs include +1 for the OR ID

// Output Report Haptic Messages for Triton
typedef struct
{
    uint8_t type;
    uint16_t intensity;
    struct
    {
        uint16_t speed;
        int8_t gain;
    } left, right;
} MsgHapticRumble;
#define HID_RUMBLE_OUTPUT_REPORT_BYTES 10


typedef struct
{
    uint8_t side;
    uint16_t on_us;
    uint16_t off_us;
    uint16_t repeat_count;
} MsgHapticPulse;
#define HID_HAPTIC_PULSE_OUTPUT_REPORT_BYTES 8

typedef struct
{
    uint8_t side;
    uint8_t command;
    int8_t gain_db;
} MsgHapticCommand;
#define HID_HAPTIC_COMMAND_REPORT_BYTES 4

typedef struct
{
    uint8_t side;
    int8_t gain_db;
    uint16_t frequency;
    uint16_t duration_ms;
    uint16_t lfo_freq;
    uint8_t lfo_depth;
} MsgHapticLfoTone;
#define HID_HAPTIC_LFO_TONE_REPORT_BYTES 10

typedef struct
{
    uint8_t side;
    int8_t gain_db;
    uint16_t duration_ms;
    struct
    {
        uint16_t frequency;
    } start, end;
} MsgHapticLogSweep;
#define HID_HAPTIC_LOG_SWEEP_REPORT_BYTES 9

typedef struct m
{
    uint8_t side;
    uint8_t script_id;
    int8_t gain_db;
} MsgHapticScript;
#define HID_HAPTIC_SCRIPT_REPORT_BYTES 4

typedef enum
{
    ID_OUT_REPORT_HAPTIC_RUMBLE		= 0x80,
    ID_OUT_REPORT_HAPTIC_PULSE		= 0x81,
    ID_OUT_REPORT_HAPTIC_COMMAND	= 0x82,
    ID_OUT_REPORT_HAPTIC_LFO_TONE	= 0x83,
    ID_OUT_REPORT_HAPTIC_LOG_SWEEP	= 0x84,
    ID_OUT_REPORT_HAPTIC_SCRIPT		= 0x85,
} ValveTritonOutReportMessageIDs;

typedef struct
{
    uint8_t report_id;
    union
    {
        MsgHapticRumble hapticRumble;
        MsgHapticPulse hapticPulse;
        MsgHapticCommand hapticCommand;
        MsgHapticLfoTone hapticLfoTone;
        MsgHapticLogSweep hapticLogSweep;
        MsgHapticScript hapticScript;
    } payload;

} OutputReportMsg;


// Roll this version forward anytime that you are breaking compatibility of existing
// message types within ValveInReport_t or the header itself.  Hopefully this should
// be super rare and instead you should just add new message payloads to the union,
// or just add fields to the end of existing payload structs which is expected to be 
// safe in all code consuming these as they should just consume/copy up to the prior size 
// they were aware of when processing.
#define k_ValveInReportMsgVersion 0x01

typedef enum
{
	ID_CONTROLLER_STATE = 1,
	ID_CONTROLLER_DEBUG = 2,
	ID_CONTROLLER_WIRELESS = 3,
	ID_CONTROLLER_STATUS = 4,
	ID_CONTROLLER_DEBUG2 = 5,
	ID_CONTROLLER_SECONDARY_STATE = 6,
	ID_CONTROLLER_BLE_STATE = 7,
	ID_CONTROLLER_DECK_STATE = 9,
	ID_CONTROLLER_MSG_COUNT
} ValveInReportMessageIDs; 

typedef struct 
{
	unsigned short unReportVersion;
	
	unsigned char ucType;
	unsigned char ucLength;
	
} ValveInReportHeader_t;

// State payload
typedef struct 
{
	// If packet num matches that on your prior call, then the controller state hasn't been changed since 
	// your last call and there is no need to process it
	uint32_t unPacketNum;
	
	// Button bitmask and trigger data.
	union
	{
		uint64_t ulButtons;
		struct
		{
			unsigned char _pad0[3];
			unsigned char nLeft;
			unsigned char nRight;
			unsigned char _pad1[3];
		} Triggers;
	} ButtonTriggerData;
	
	// Left pad coordinates
	short sLeftPadX;
	short sLeftPadY;
	
	// Right pad coordinates
	short sRightPadX;
	short sRightPadY;
	
	// This is redundant, packed above, but still sent over wired
	unsigned short sTriggerL;
	unsigned short sTriggerR;

	// FIXME figure out a way to grab this stuff over wireless
	short sAccelX;
	short sAccelY;
	short sAccelZ;
	
	short sGyroX;
	short sGyroY;
	short sGyroZ;
	
	short sGyroQuatW;
	short sGyroQuatX;
	short sGyroQuatY;
	short sGyroQuatZ;

} ValveControllerStatePacket_t;

// BLE State payload this has to be re-formatted from the normal state because BLE controller shows up as 
//a HID device and we don't want to send all the optional parts of the message. Keep in sync with struct above.
typedef struct
{
	// If packet num matches that on your prior call, then the controller state hasn't been changed since 
	// your last call and there is no need to process it
	uint32_t unPacketNum;

	// Button bitmask and trigger data.
	union
	{
		uint64_t ulButtons;
		struct
		{
			unsigned char _pad0[3];
			unsigned char nLeft;
			unsigned char nRight;
			unsigned char _pad1[3];
		} Triggers;
	} ButtonTriggerData;

	// Left pad coordinates
	short sLeftPadX;
	short sLeftPadY;

	// Right pad coordinates
	short sRightPadX;
	short sRightPadY;

	//This mimics how the dongle reconstitutes HID packets, there will be 0-4 shorts depending on gyro mode
	unsigned char ucGyroDataType; //TODO could maybe find some unused bits in the button field for this info (is only 2bits)
	short sGyro[4];

} ValveControllerBLEStatePacket_t;

// Define a payload for reporting debug information
typedef struct
{
	// Left pad coordinates
	short sLeftPadX;
	short sLeftPadY;

	// Right pad coordinates
	short sRightPadX;
	short sRightPadY;

	// Left mouse deltas
	short sLeftPadMouseDX;
	short sLeftPadMouseDY;

	// Right mouse deltas
	short sRightPadMouseDX;
	short sRightPadMouseDY;
	
	// Left mouse filtered deltas
	short sLeftPadMouseFilteredDX;
	short sLeftPadMouseFilteredDY;

	// Right mouse filtered deltas
	short sRightPadMouseFilteredDX;
	short sRightPadMouseFilteredDY;
	
	// Pad Z values
	unsigned char ucLeftZ;
	unsigned char ucRightZ;
	
	// FingerPresent
	unsigned char ucLeftFingerPresent;
	unsigned char ucRightFingerPresent;
	
	// Timestamps
	unsigned char ucLeftTimestamp;
	unsigned char ucRightTimestamp;
	
	// Double tap state
	unsigned char ucLeftTapState;
	unsigned char ucRightTapState;
	
	unsigned int unDigitalIOStates0;
	unsigned int unDigitalIOStates1;
	
} ValveControllerDebugPacket_t;

typedef struct
{
	unsigned char ucPadNum;
	unsigned char ucPad[3]; // need Data to be word aligned
	short Data[20];
	unsigned short unNoise;
} ValveControllerTrackpadImage_t;

typedef struct
{
	unsigned char ucPadNum;
	unsigned char ucOffset;
	unsigned char ucPad[2]; // need Data to be word aligned
	short rgData[28];
} ValveControllerRawTrackpadImage_t;

// Payload for wireless metadata
typedef struct 
{
	unsigned char ucEventType;
} SteamControllerWirelessEvent_t;

typedef struct 
{
	// Current packet number.
    unsigned int unPacketNum;
	
	// Event codes and state information.
    unsigned short sEventCode;
    unsigned short unStateFlags;

    // Current battery voltage (mV).
    unsigned short sBatteryVoltage;
	
	// Current battery level (0-100).
	unsigned char ucBatteryLevel;
} SteamControllerStatusEvent_t;

// Deck State payload
typedef struct
{
	// If packet num matches that on your prior call, then the controller
	// state hasn't been changed since your last call and there is no need to
	// process it
	uint32_t unPacketNum;

	// Button bitmask and trigger data.
	union
	{
		uint64_t ulButtons;
		struct
		{
			uint32_t ulButtonsL;
			uint32_t ulButtonsH;
		};
	};

	// Left pad coordinates
	short sLeftPadX;
	short sLeftPadY;

	// Right pad coordinates
	short sRightPadX;
	short sRightPadY;

	// Accelerometer values
	short sAccelX;
	short sAccelY;
	short sAccelZ;

	// Gyroscope values
	short sGyroX;
	short sGyroY;
	short sGyroZ;

	// Gyro quaternions
	short sGyroQuatW;
	short sGyroQuatX;
	short sGyroQuatY;
	short sGyroQuatZ;

	// Uncalibrated trigger values
	unsigned short sTriggerRawL;
	unsigned short sTriggerRawR;

	// Left stick values
	short sLeftStickX;
	short sLeftStickY;

	// Right stick values
	short sRightStickX;
	short sRightStickY;

	// Touchpad pressures
	unsigned short sPressurePadLeft;
	unsigned short sPressurePadRight;
} SteamDeckStatePacket_t;


typedef struct
{
    ValveInReportHeader_t header;

    union
    {
        ValveControllerStatePacket_t controllerState;
        ValveControllerBLEStatePacket_t controllerBLEState;
        ValveControllerDebugPacket_t debugState;
        ValveControllerTrackpadImage_t padImage;
        ValveControllerRawTrackpadImage_t rawPadImage;
        SteamControllerWirelessEvent_t wirelessEvent;
        SteamControllerStatusEvent_t statusEvent;
        SteamDeckStatePacket_t deckState;
    } payload;

} ValveInReport_t;

enum EBLEPacketReportNums
{
	k_EBLEReportState	= 4,
	k_EBLEReportStatus	= 5,
};
// Enumeration of data chunks in BLE state packets
enum EBLEOptionDataChunksBitmask
{
    // First byte upper nibble
    k_EBLEButtonChunk1 = 0x10,
    k_EBLEButtonChunk2 = 0x20,
    k_EBLEButtonChunk3 = 0x40,
    k_EBLELeftJoystickChunk = 0x80,

    // Second full byte
    k_EBLELeftTrackpadChunk = 0x100,
    k_EBLERightTrackpadChunk = 0x200,
    k_EBLEIMUAccelChunk = 0x400,
    k_EBLEIMUGyroChunk = 0x800,
    k_EBLEIMUQuatChunk = 0x1000,
};

// Triton and derivatives do not use the ValveInReport_t structure

enum ETritonReportIDTypes
{
    ID_TRITON_CONTROLLER_STATE	= 0x42,
    ID_TRITON_BATTERY_STATUS	= 0x43,
    ID_TRITON_CONTROLLER_STATE_BLE = 0x45,
    ID_TRITON_WIRELESS_STATUS_X = 0x46,
    ID_TRITON_CONTROLLER_STATE_TIMESTAMP = 0x47,

    ID_TRITON_WIRELESS_STATUS   = 0x79,
};

enum ETritonWirelessState
{
    k_ETritonWirelessStateDisconnect = 1,
	k_ETritonWirelessStateConnect = 2,
};

typedef struct
{
    uint32_t timestamp;
    short sAccelX;
    short sAccelY;
    short sAccelZ;

    short sGyroX;
    short sGyroY;
    short sGyroZ;

    short sGyroQuatW;
    short sGyroQuatX;
    short sGyroQuatY;
    short sGyroQuatZ;
} TritonMTUIMU_t;

typedef struct {
	uint32_t timestamp;
	short sAccelX;
	short sAccelY;
	short sAccelZ;

	short sGyroX;
	short sGyroY;
	short sGyroZ;
} TritonMTUIMUNoQuat_t;

typedef struct
{
    uint16_t timestamp;
    short sAccelX;
    short sAccelY;
    short sAccelZ;

    short sGyroX;
    short sGyroY;
    short sGyroZ;
} TritonMTUIMUNoQuat32usTS_t;

typedef struct
{
    uint8_t report_id; //added
    uint8_t seq_num;
    uint32_t buttons;
    short sTriggerLeft;
    short sTriggerRight;

    short sLeftStickX;
    short sLeftStickY;
    short sRightStickX;
    short sRightStickY;

    short sLeftPadX;
    short sLeftPadY;
    unsigned short unPressureLeft;

    short sRightPadX;
    short sRightPadY;
    unsigned short unPressureRight;
    TritonMTUIMU_t imu;
} TritonMTUFull_t;

typedef struct {
	uint8_t seq_num;
	uint32_t buttons;
	short sTriggerLeft;
	short sTriggerRight;

	short sLeftStickX;
	short sLeftStickY;
	short sRightStickX;
	short sRightStickY;

	short sLeftPadX;
	short sLeftPadY;
	unsigned short unPressureLeft;

	short sRightPadX;
	short sRightPadY;
	unsigned short unPressureRight;
	TritonMTUIMUNoQuat_t imu;
} TritonMTUNoQuat_t;

// New Ibex packet that adds a timestamp to the trackpad sampling
// and reduces the size of the IMU timestamp.  Timestamps are now 16 bits
typedef struct
{
    uint8_t seq_num;
    uint32_t buttons;
    short sTriggerLeft;
    short sTriggerRight;

    short sLeftStickX;
    short sLeftStickY;
    short sRightStickX;
    short sRightStickY;

    unsigned short unTrackpadTimestamp;
    short sLeftPadX;
    short sLeftPadY;
    unsigned short unPressureLeft;

    short sRightPadX;
    short sRightPadY;
    unsigned short unPressureRight;

    TritonMTUIMUNoQuat32usTS_t imu;
} TritonMTUNoQuat32TS_t;

enum EChargeState
{
    k_EChargeStateReset,
    k_EChargeStateDischarging,
    k_EChargeStateCharging,
    k_EChargeStateSrcValidate,
    k_EChargeStateChargingDone,
};

typedef struct
{
    unsigned char ucChargeState; // EChargeState
    unsigned char ucBatteryLevel;
    unsigned short sBatteryVoltage;
    unsigned short sSystemVoltage;
    unsigned short sInputVoltage;
    unsigned short sCurrent;
    unsigned short sInputCurrent;
    unsigned short sTemperature;
} TritonBatteryStatus_t;

typedef struct
{
    unsigned char state;
} TritonWirelessStatus_t;

#pragma pack()

#endif // _CONTROLLER_STRUCTS_

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

#ifndef _CONTROLLER_CONSTANTS_
#define _CONTROLLER_CONSTANTS_

#ifdef __cplusplus
extern "C" {
#endif

#define FEATURE_REPORT_SIZE	64

#define VALVE_USB_VID		0x28DE

// Frame update rate (in ms).
#define FAST_SCAN_INTERVAL  6
#define SLOW_SCAN_INTERVAL  9

// Contains each of the USB PIDs for Valve controllers (only add to this enum and never change the order)
enum ValveControllerPID
{
	BASTILLE_PID              = 0x2202,
	CHELL_PID                 = 0x1101,
	D0G_PID                   = 0x1102,
	ELI_PID                   = 0x1103,
	FREEMAN_PID               = 0x1104,
	D0G_BLE_PID				  = 0x1105,
	D0G_BLE2_PID			  = 0x1106,
	D0GGLE_PID                = 0x1142,

	JUPITER_PID               = 0x1205,
};

// This enum contains all of the messages exchanged between the host and the target (only add to this enum and never change the order)
enum FeatureReportMessageIDs
{
	ID_SET_DIGITAL_MAPPINGS              = 0x80,
	ID_CLEAR_DIGITAL_MAPPINGS            = 0x81,
	ID_GET_DIGITAL_MAPPINGS              = 0x82,
	ID_GET_ATTRIBUTES_VALUES             = 0x83,
	ID_GET_ATTRIBUTE_LABEL               = 0x84,
	ID_SET_DEFAULT_DIGITAL_MAPPINGS      = 0x85,
	ID_FACTORY_RESET                     = 0x86,
	ID_SET_SETTINGS_VALUES               = 0x87,
	ID_CLEAR_SETTINGS_VALUES             = 0x88,
	ID_GET_SETTINGS_VALUES               = 0x89,
	ID_GET_SETTING_LABEL                 = 0x8A,
	ID_GET_SETTINGS_MAXS                 = 0x8B,
	ID_GET_SETTINGS_DEFAULTS             = 0x8C,
	ID_SET_CONTROLLER_MODE               = 0x8D,
	ID_LOAD_DEFAULT_SETTINGS             = 0x8E,
	ID_TRIGGER_HAPTIC_PULSE              = 0x8F,

	ID_TURN_OFF_CONTROLLER               = 0x9F,

	ID_GET_DEVICE_INFO                   = 0xA1,

	ID_CALIBRATE_TRACKPADS               = 0xA7,
	ID_RESERVED_0                        = 0xA8,
	ID_SET_SERIAL_NUMBER                 = 0xA9,
	ID_GET_TRACKPAD_CALIBRATION          = 0xAA,
	ID_GET_TRACKPAD_FACTORY_CALIBRATION  = 0xAB,
	ID_GET_TRACKPAD_RAW_DATA             = 0xAC,
	ID_ENABLE_PAIRING                    = 0xAD,
	ID_GET_STRING_ATTRIBUTE              = 0xAE,
	ID_RADIO_ERASE_RECORDS               = 0xAF,
	ID_RADIO_WRITE_RECORD                = 0xB0,
	ID_SET_DONGLE_SETTING                = 0xB1,
	ID_DONGLE_DISCONNECT_DEVICE          = 0xB2,
	ID_DONGLE_COMMIT_DEVICE              = 0xB3,
	ID_DONGLE_GET_WIRELESS_STATE         = 0xB4,
	ID_CALIBRATE_GYRO                    = 0xB5,
	ID_PLAY_AUDIO                        = 0xB6,
	ID_AUDIO_UPDATE_START                = 0xB7,
	ID_AUDIO_UPDATE_DATA                 = 0xB8,
	ID_AUDIO_UPDATE_COMPLETE             = 0xB9,
	ID_GET_CHIPID                        = 0xBA,

	ID_CALIBRATE_JOYSTICK                = 0xBF,
	ID_CALIBRATE_ANALOG_TRIGGERS         = 0xC0,
	ID_SET_AUDIO_MAPPING                 = 0xC1,
	ID_CHECK_GYRO_FW_LOAD                = 0xC2,
	ID_CALIBRATE_ANALOG                  = 0xC3,
	ID_DONGLE_GET_CONNECTED_SLOTS        = 0xC4,

	ID_RESET_IMU                         = 0xCE,

	// Deck only
	ID_TRIGGER_HAPTIC_CMD                = 0xEA,
	ID_TRIGGER_RUMBLE_CMD                = 0xEB,
};


// Enumeration of all wireless dongle events
typedef enum WirelessEventTypes
{
	WIRELESS_EVENT_DISCONNECT	= 1,
	WIRELESS_EVENT_CONNECT		= 2,
	WIRELESS_EVENT_PAIR			= 3,
} EWirelessEventType;


// Enumeration of generic digital inputs - not all of these will be supported on all controllers (only add to this enum and never change the order)
typedef enum
{
	IO_DIGITAL_BUTTON_NONE = -1,
	IO_DIGITAL_BUTTON_RIGHT_TRIGGER,
	IO_DIGITAL_BUTTON_LEFT_TRIGGER,
	IO_DIGITAL_BUTTON_1,
	IO_DIGITAL_BUTTON_Y=IO_DIGITAL_BUTTON_1,
	IO_DIGITAL_BUTTON_2,
	IO_DIGITAL_BUTTON_B=IO_DIGITAL_BUTTON_2,
	IO_DIGITAL_BUTTON_3,
	IO_DIGITAL_BUTTON_X=IO_DIGITAL_BUTTON_3,
	IO_DIGITAL_BUTTON_4,
	IO_DIGITAL_BUTTON_A=IO_DIGITAL_BUTTON_4,
	IO_DIGITAL_BUTTON_RIGHT_BUMPER,
	IO_DIGITAL_BUTTON_LEFT_BUMPER,
	IO_DIGITAL_BUTTON_LEFT_JOYSTICK_CLICK,
	IO_DIGITAL_BUTTON_ESCAPE,
	IO_DIGITAL_BUTTON_STEAM,
	IO_DIGITAL_BUTTON_MENU,
	IO_DIGITAL_STICK_UP,
	IO_DIGITAL_STICK_DOWN,
	IO_DIGITAL_STICK_LEFT,
	IO_DIGITAL_STICK_RIGHT,
	IO_DIGITAL_TOUCH_1,
	IO_DIGITAL_BUTTON_UP=IO_DIGITAL_TOUCH_1,
	IO_DIGITAL_TOUCH_2,
	IO_DIGITAL_BUTTON_RIGHT=IO_DIGITAL_TOUCH_2,
	IO_DIGITAL_TOUCH_3,
	IO_DIGITAL_BUTTON_LEFT=IO_DIGITAL_TOUCH_3,
	IO_DIGITAL_TOUCH_4,
	IO_DIGITAL_BUTTON_DOWN=IO_DIGITAL_TOUCH_4,
	IO_DIGITAL_BUTTON_BACK_LEFT,
	IO_DIGITAL_BUTTON_BACK_RIGHT,
	IO_DIGITAL_LEFT_TRACKPAD_N,
	IO_DIGITAL_LEFT_TRACKPAD_NE,
	IO_DIGITAL_LEFT_TRACKPAD_E,
	IO_DIGITAL_LEFT_TRACKPAD_SE,
	IO_DIGITAL_LEFT_TRACKPAD_S,
	IO_DIGITAL_LEFT_TRACKPAD_SW,
	IO_DIGITAL_LEFT_TRACKPAD_W,
	IO_DIGITAL_LEFT_TRACKPAD_NW,
	IO_DIGITAL_RIGHT_TRACKPAD_N,
	IO_DIGITAL_RIGHT_TRACKPAD_NE,
	IO_DIGITAL_RIGHT_TRACKPAD_E,
	IO_DIGITAL_RIGHT_TRACKPAD_SE,
	IO_DIGITAL_RIGHT_TRACKPAD_S,
	IO_DIGITAL_RIGHT_TRACKPAD_SW,
	IO_DIGITAL_RIGHT_TRACKPAD_W,
	IO_DIGITAL_RIGHT_TRACKPAD_NW,
	IO_DIGITAL_LEFT_TRACKPAD_DOUBLE_TAP,
	IO_DIGITAL_RIGHT_TRACKPAD_DOUBLE_TAP,
	IO_DIGITAL_LEFT_TRACKPAD_OUTER_RADIUS,
	IO_DIGITAL_RIGHT_TRACKPAD_OUTER_RADIUS,
	IO_DIGITAL_LEFT_TRACKPAD_CLICK,
	IO_DIGITAL_RIGHT_TRACKPAD_CLICK,
	IO_DIGITAL_BATTERY_LOW,
	IO_DIGITAL_LEFT_TRIGGER_THRESHOLD,
	IO_DIGITAL_RIGHT_TRIGGER_THRESHOLD,
	IO_DIGITAL_BUTTON_BACK_LEFT2,
	IO_DIGITAL_BUTTON_BACK_RIGHT2,
	IO_DIGITAL_BUTTON_ALWAYS_ON,
	IO_DIGITAL_BUTTON_ANCILLARY_1,
	IO_DIGITAL_BUTTON_MACRO_0,
	IO_DIGITAL_BUTTON_MACRO_1,
	IO_DIGITAL_BUTTON_MACRO_2,
	IO_DIGITAL_BUTTON_MACRO_3,
	IO_DIGITAL_BUTTON_MACRO_4,
	IO_DIGITAL_BUTTON_MACRO_5,
	IO_DIGITAL_BUTTON_MACRO_6,
	IO_DIGITAL_BUTTON_MACRO_7,
	IO_DIGITAL_BUTTON_MACRO_1FINGER,
	IO_DIGITAL_BUTTON_MACRO_2FINGER,
	IO_DIGITAL_COUNT
} DigitalIO ;

// Enumeration of generic analog inputs - not all of these will be supported on all controllers (only add to this enum and never change the order)
typedef enum 
{
	IO_ANALOG_LEFT_STICK_X,
	IO_ANALOG_LEFT_STICK_Y,
	IO_ANALOG_RIGHT_STICK_X,
	IO_ANALOG_RIGHT_STICK_Y,
	IO_ANALOG_LEFT_TRIGGER,
	IO_ANALOG_RIGHT_TRIGGER,
	IO_MOUSE1_X,
	IO_MOUSE1_Y,
	IO_MOUSE1_Z,
	IO_ACCEL_X,
	IO_ACCEL_Y,
	IO_ACCEL_Z,
	IO_GYRO_X,
	IO_GYRO_Y,
	IO_GYRO_Z,
	IO_GYRO_QUAT_W,
	IO_GYRO_QUAT_X,
	IO_GYRO_QUAT_Y,
	IO_GYRO_QUAT_Z,
	IO_GYRO_STEERING_VEC,
	IO_RAW_TRIGGER_LEFT,
	IO_RAW_TRIGGER_RIGHT,
	IO_RAW_JOYSTICK_X,
	IO_RAW_JOYSTICK_Y,
	IO_GYRO_TILT_VEC,
	IO_PRESSURE_LEFT_PAD,
	IO_PRESSURE_RIGHT_PAD,
	IO_PRESSURE_LEFT_BUMPER,
	IO_PRESSURE_RIGHT_BUMPER,
	IO_PRESSURE_LEFT_GRIP,
	IO_PRESSURE_RIGHT_GRIP,
	IO_ANALOG_LEFT_TRIGGER_THRESHOLD,
	IO_ANALOG_RIGHT_TRIGGER_THRESHOLD,
	IO_PRESSURE_RIGHT_PAD_THRESHOLD,
	IO_PRESSURE_LEFT_PAD_THRESHOLD,
	IO_PRESSURE_RIGHT_BUMPER_THRESHOLD,
	IO_PRESSURE_LEFT_BUMPER_THRESHOLD,
	IO_PRESSURE_RIGHT_GRIP_THRESHOLD,
	IO_PRESSURE_LEFT_GRIP_THRESHOLD,
	IO_PRESSURE_RIGHT_PAD_RAW,
	IO_PRESSURE_LEFT_PAD_RAW,
	IO_PRESSURE_RIGHT_BUMPER_RAW,
	IO_PRESSURE_LEFT_BUMPER_RAW,
	IO_PRESSURE_RIGHT_GRIP_RAW,
	IO_PRESSURE_LEFT_GRIP_RAW,
	IO_PRESSURE_RIGHT_GRIP2_THRESHOLD,
	IO_PRESSURE_LEFT_GRIP2_THRESHOLD,
	IO_PRESSURE_LEFT_GRIP2,
	IO_PRESSURE_RIGHT_GRIP2,
	IO_PRESSURE_RIGHT_GRIP2_RAW,
	IO_PRESSURE_LEFT_GRIP2_RAW,
	IO_ANALOG_COUNT
} AnalogIO;


// Contains list of all types of devices that the controller emulates (only add to this enum and never change the order)
enum DeviceTypes
{
	DEVICE_KEYBOARD,
	DEVICE_MOUSE,
	DEVICE_GAMEPAD,
	DEVICE_MODE_ADJUST,
	DEVICE_COUNT
};

// Scan codes for HID keyboards 
enum HIDKeyboardKeys
{
	KEY_INVALID,
	KEY_FIRST = 0x04,
	KEY_A = KEY_FIRST, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, 
	KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z, KEY_1, KEY_2, 
	KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_RETURN, KEY_ESCAPE, KEY_BACKSPACE, KEY_TAB, KEY_SPACE, KEY_DASH, KEY_EQUALS, KEY_LEFT_BRACKET,
	KEY_RIGHT_BRACKET, KEY_BACKSLASH, KEY_UNUSED1, KEY_SEMICOLON, KEY_SINGLE_QUOTE, KEY_BACK_TICK, KEY_COMMA, KEY_PERIOD, KEY_FORWARD_SLASH, KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
	KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_PRINT_SCREEN, KEY_SCROLL_LOCK, KEY_BREAK, KEY_INSERT, KEY_HOME, KEY_PAGE_UP, KEY_DELETE, KEY_END, KEY_PAGE_DOWN, KEY_RIGHT_ARROW,
	KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_UP_ARROW, KEY_NUM_LOCK, KEY_KEYPAD_FORWARD_SLASH, KEY_KEYPAD_ASTERISK, KEY_KEYPAD_DASH, KEY_KEYPAD_PLUS, KEY_KEYPAD_ENTER, KEY_KEYPAD_1, KEY_KEYPAD_2, KEY_KEYPAD_3, KEY_KEYPAD_4, KEY_KEYPAD_5, KEY_KEYPAD_6, KEY_KEYPAD_7,
	KEY_KEYPAD_8, KEY_KEYPAD_9, KEY_KEYPAD_0, KEY_KEYPAD_PERIOD,
	KEY_LALT,
    KEY_LSHIFT,
    KEY_LWIN,
    KEY_LCONTROL,
    KEY_RALT,
    KEY_RSHIFT,
    KEY_RWIN,
    KEY_RCONTROL,
	KEY_VOLUP,
	KEY_VOLDOWN,
	KEY_MUTE,
	KEY_PLAY,
	KEY_STOP,
	KEY_NEXT,
	KEY_PREV,
    KEY_LAST = KEY_PREV
};

enum ModifierMasks
{
  KEY_LCONTROL_MASK = (1<<0),
  KEY_LSHIFT_MASK = (1<<1),
  KEY_LALT_MASK = (1<<2),
  KEY_LWIN_MASK = (1<<3),
  KEY_RCONTROL_MASK = (1<<4),
  KEY_RSHIFT_MASK = (1<<5),
  KEY_RALT_MASK = (1<<6),
  KEY_RWIN_MASK = (1<<7)
};

// Standard mouse buttons as specified in the HID mouse spec
enum MouseButtons
{
	MOUSE_BTN_LEFT,
	MOUSE_BTN_RIGHT,
	MOUSE_BTN_MIDDLE,
	MOUSE_BTN_BACK,
	MOUSE_BTN_FORWARD,
	MOUSE_SCROLL_UP,
	MOUSE_SCROLL_DOWN,
	MOUSE_BTN_COUNT
};

// Gamepad buttons
enum GamepadButtons
{
	GAMEPAD_BTN_TRIGGER_LEFT=1, 
	GAMEPAD_BTN_TRIGGER_RIGHT,
	GAMEPAD_BTN_A,
	GAMEPAD_BTN_B,
	GAMEPAD_BTN_Y,
	GAMEPAD_BTN_X,
	GAMEPAD_BTN_SHOULDER_LEFT,
	GAMEPAD_BTN_SHOULDER_RIGHT,
	GAMEPAD_BTN_LEFT_JOYSTICK,
	GAMEPAD_BTN_RIGHT_JOYSTICK,
	GAMEPAD_BTN_START,
	GAMEPAD_BTN_SELECT,
	GAMEPAD_BTN_STEAM,
	GAMEPAD_BTN_DPAD_UP,
	GAMEPAD_BTN_DPAD_DOWN,
	GAMEPAD_BTN_DPAD_LEFT,
	GAMEPAD_BTN_DPAD_RIGHT,
	GAMEPAD_BTN_LSTICK_UP,
	GAMEPAD_BTN_LSTICK_DOWN,
	GAMEPAD_BTN_LSTICK_LEFT,
	GAMEPAD_BTN_LSTICK_RIGHT,
	GAMEPAD_BTN_RSTICK_UP,
	GAMEPAD_BTN_RSTICK_DOWN,
	GAMEPAD_BTN_RSTICK_LEFT,
	GAMEPAD_BTN_RSTICK_RIGHT,
	GAMEPAD_BTN_COUNT
};

// Mode adjust
enum ModeAdjustModes
{
	MODE_ADJUST_SENSITIVITY=1,
	MODE_ADJUST_LEFT_PAD_SECONDARY_MODE,
	MODE_ADJUST_RIGHT_PAD_SECONDARY_MODE,
	MODE_ADJUST_COUNT
};

// Read-only attributes of controllers (only add to this enum and never change the order)
typedef enum
{
	ATTRIB_UNIQUE_ID,
	ATTRIB_PRODUCT_ID,
	ATTRIB_PRODUCT_REVISION,											// deprecated
	ATTRIB_CAPABILITIES = ATTRIB_PRODUCT_REVISION,	// intentional aliasing
	ATTRIB_FIRMWARE_VERSION,										// deprecated
	ATTRIB_FIRMWARE_BUILD_TIME,
	ATTRIB_RADIO_FIRMWARE_BUILD_TIME,
	ATTRIB_RADIO_DEVICE_ID0,
	ATTRIB_RADIO_DEVICE_ID1,
	ATTRIB_DONGLE_FIRMWARE_BUILD_TIME,
	ATTRIB_BOARD_REVISION,
	ATTRIB_BOOTLOADER_BUILD_TIME,
	ATTRIB_CONNECTION_INTERVAL_IN_US,
	ATTRIB_COUNT
} ControllerAttributes;

// Read-only string attributes of controllers (only add to this enum and never change the order)
typedef enum
{
	ATTRIB_STR_BOARD_SERIAL,
	ATTRIB_STR_UNIT_SERIAL,
	ATTRIB_STR_COUNT
} ControllerStringAttributes;

typedef enum
{
	STATUS_CODE_NORMAL,
	STATUS_CODE_CRITICAL_BATTERY,
	STATUS_CODE_GYRO_INIT_ERROR,
} ControllerStatusEventCodes;

typedef enum
{
	STATUS_STATE_LOW_BATTERY=0,
} ControllerStatusStateFlags;

typedef enum {
	TRACKPAD_ABSOLUTE_MOUSE,
	TRACKPAD_RELATIVE_MOUSE,
	TRACKPAD_DPAD_FOUR_WAY_DISCRETE,
	TRACKPAD_DPAD_FOUR_WAY_OVERLAP,
	TRACKPAD_DPAD_EIGHT_WAY,
	TRACKPAD_RADIAL_MODE,
	TRACKPAD_ABSOLUTE_DPAD,
	TRACKPAD_NONE,
	TRACKPAD_GESTURE_KEYBOARD,
	TRACKPAD_NUM_MODES
} TrackpadDPadMode;

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
	SETTING_ALL=0xFF
} ControllerSettings;

typedef enum
{
	SETTING_DEFAULT,
	SETTING_MIN,
	SETTING_MAX,
	SETTING_DEFAULTMINMAXCOUNT
} SettingDefaultMinMax;

// Bitmask that define which IMU features to enable.
typedef enum
{
	SETTING_GYRO_MODE_OFF				= 0x0000,
	SETTING_GYRO_MODE_STEERING			= 0x0001,
	SETTING_GYRO_MODE_TILT				= 0x0002,
	SETTING_GYRO_MODE_SEND_ORIENTATION	= 0x0004,
	SETTING_GYRO_MODE_SEND_RAW_ACCEL	= 0x0008,
	SETTING_GYRO_MODE_SEND_RAW_GYRO		= 0x0010,
} SettingGyroMode;

// Bitmask for haptic pulse flags
typedef enum
{
	HAPTIC_PULSE_NORMAL					= 0x0000,
	HAPTIC_PULSE_HIGH_PRIORITY			= 0x0001,
	HAPTIC_PULSE_VERY_HIGH_PRIORITY		= 0x0002,
	HAPTIC_PULSE_IGNORE_USER_PREFS		= 0x0003,
} SettingHapticPulseFlags;

typedef struct
{
	// default,min,max in this array in that order
	short defaultminmax[SETTING_DEFAULTMINMAXCOUNT]; 
} SettingValueRange_t;

// below is from controller_constants.c which should be compiled into any code that uses this
extern const SettingValueRange_t g_DefaultSettingValues[SETTING_COUNT];

// Read-write settings for dongle (only add to this enum and never change the order)
typedef enum 
{
	DONGLE_SETTING_MOUSE_KEYBOARD_ENABLED,
	DONGLE_SETTING_COUNT,
} DongleSettings;

typedef enum
{
	AUDIO_STARTUP		= 0,
	AUDIO_SHUTDOWN		= 1,
	AUDIO_PAIR			= 2,
	AUDIO_PAIR_SUCCESS	= 3,
	AUDIO_IDENTIFY		= 4,
	AUDIO_LIZARDMODE	= 5,
	AUDIO_NORMALMODE	= 6,

	AUDIO_MAX_SLOT      = 15
} ControllerAudio;

#ifdef __cplusplus
}
#endif

#endif // _CONTROLLER_CONSTANTS_
