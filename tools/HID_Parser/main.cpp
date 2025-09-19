#include <iostream>
#include <string>
#include <fstream>
#include <cstdio>
#include <cinttypes>
#include <vector>
#include <algorithm>
#include <varargs.h>

/* --------------------------------------------------- 

                    LIBNX Hid.h

  --------------------------------------------------- */

#ifndef BIT
#define BIT(n) (1U<<(n))
#endif

#ifndef BITL
#define BITL(n) (1UL<<(n))
#endif

typedef struct { float value[3]; } UtilFloat3;   ///< 3 floats.

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

/// HidDebugPadButton
typedef enum {
    HidDebugPadButton_A             = BIT(0),  ///< A button
    HidDebugPadButton_B             = BIT(1),  ///< B button
    HidDebugPadButton_X             = BIT(2),  ///< X button
    HidDebugPadButton_Y             = BIT(3),  ///< Y button
    HidDebugPadButton_L             = BIT(4),  ///< L button
    HidDebugPadButton_R             = BIT(5),  ///< R button
    HidDebugPadButton_ZL            = BIT(6),  ///< ZL button
    HidDebugPadButton_ZR            = BIT(7),  ///< ZR button
    HidDebugPadButton_Start         = BIT(8),  ///< Start button
    HidDebugPadButton_Select        = BIT(9),  ///< Select button
    HidDebugPadButton_Left          = BIT(10), ///< D-Pad Left button
    HidDebugPadButton_Up            = BIT(11), ///< D-Pad Up button
    HidDebugPadButton_Right         = BIT(12), ///< D-Pad Right button
    HidDebugPadButton_Down          = BIT(13), ///< D-Pad Down button
} HidDebugPadButton;

/// HidTouchScreenModeForNx
typedef enum {
    HidTouchScreenModeForNx_UseSystemSetting     = 0,     ///< UseSystemSetting
    HidTouchScreenModeForNx_Finger               = 1,     ///< Finger
    HidTouchScreenModeForNx_Heat2                = 2,     ///< Heat2
} HidTouchScreenModeForNx;

/// HidMouseButton
typedef enum {
    HidMouseButton_Left    = BIT(0),
    HidMouseButton_Right   = BIT(1),
    HidMouseButton_Middle  = BIT(2),
    HidMouseButton_Forward = BIT(3),
    HidMouseButton_Back    = BIT(4),
} HidMouseButton;

/// HidKeyboardKey
typedef enum {
    HidKeyboardKey_A                = 4,
    HidKeyboardKey_B                = 5,
    HidKeyboardKey_C                = 6,
    HidKeyboardKey_D                = 7,
    HidKeyboardKey_E                = 8,
    HidKeyboardKey_F                = 9,
    HidKeyboardKey_G                = 10,
    HidKeyboardKey_H                = 11,
    HidKeyboardKey_I                = 12,
    HidKeyboardKey_J                = 13,
    HidKeyboardKey_K                = 14,
    HidKeyboardKey_L                = 15,
    HidKeyboardKey_M                = 16,
    HidKeyboardKey_N                = 17,
    HidKeyboardKey_O                = 18,
    HidKeyboardKey_P                = 19,
    HidKeyboardKey_Q                = 20,
    HidKeyboardKey_R                = 21,
    HidKeyboardKey_S                = 22,
    HidKeyboardKey_T                = 23,
    HidKeyboardKey_U                = 24,
    HidKeyboardKey_V                = 25,
    HidKeyboardKey_W                = 26,
    HidKeyboardKey_X                = 27,
    HidKeyboardKey_Y                = 28,
    HidKeyboardKey_Z                = 29,
    HidKeyboardKey_D1               = 30,
    HidKeyboardKey_D2               = 31,
    HidKeyboardKey_D3               = 32,
    HidKeyboardKey_D4               = 33,
    HidKeyboardKey_D5               = 34,
    HidKeyboardKey_D6               = 35,
    HidKeyboardKey_D7               = 36,
    HidKeyboardKey_D8               = 37,
    HidKeyboardKey_D9               = 38,
    HidKeyboardKey_D0               = 39,
    HidKeyboardKey_Return           = 40,
    HidKeyboardKey_Escape           = 41,
    HidKeyboardKey_Backspace        = 42,
    HidKeyboardKey_Tab              = 43,
    HidKeyboardKey_Space            = 44,
    HidKeyboardKey_Minus            = 45,
    HidKeyboardKey_Plus             = 46,
    HidKeyboardKey_OpenBracket      = 47,
    HidKeyboardKey_CloseBracket     = 48,
    HidKeyboardKey_Pipe             = 49,
    HidKeyboardKey_Tilde            = 50,
    HidKeyboardKey_Semicolon        = 51,
    HidKeyboardKey_Quote            = 52,
    HidKeyboardKey_Backquote        = 53,
    HidKeyboardKey_Comma            = 54,
    HidKeyboardKey_Period           = 55,
    HidKeyboardKey_Slash            = 56,
    HidKeyboardKey_CapsLock         = 57,
    HidKeyboardKey_F1               = 58,
    HidKeyboardKey_F2               = 59,
    HidKeyboardKey_F3               = 60,
    HidKeyboardKey_F4               = 61,
    HidKeyboardKey_F5               = 62,
    HidKeyboardKey_F6               = 63,
    HidKeyboardKey_F7               = 64,
    HidKeyboardKey_F8               = 65,
    HidKeyboardKey_F9               = 66,
    HidKeyboardKey_F10              = 67,
    HidKeyboardKey_F11              = 68,
    HidKeyboardKey_F12              = 69,
    HidKeyboardKey_PrintScreen      = 70,
    HidKeyboardKey_ScrollLock       = 71,
    HidKeyboardKey_Pause            = 72,
    HidKeyboardKey_Insert           = 73,
    HidKeyboardKey_Home             = 74,
    HidKeyboardKey_PageUp           = 75,
    HidKeyboardKey_Delete           = 76,
    HidKeyboardKey_End              = 77,
    HidKeyboardKey_PageDown         = 78,
    HidKeyboardKey_RightArrow       = 79,
    HidKeyboardKey_LeftArrow        = 80,
    HidKeyboardKey_DownArrow        = 81,
    HidKeyboardKey_UpArrow          = 82,
    HidKeyboardKey_NumLock          = 83,
    HidKeyboardKey_NumPadDivide     = 84,
    HidKeyboardKey_NumPadMultiply   = 85,
    HidKeyboardKey_NumPadSubtract   = 86,
    HidKeyboardKey_NumPadAdd        = 87,
    HidKeyboardKey_NumPadEnter      = 88,
    HidKeyboardKey_NumPad1          = 89,
    HidKeyboardKey_NumPad2          = 90,
    HidKeyboardKey_NumPad3          = 91,
    HidKeyboardKey_NumPad4          = 92,
    HidKeyboardKey_NumPad5          = 93,
    HidKeyboardKey_NumPad6          = 94,
    HidKeyboardKey_NumPad7          = 95,
    HidKeyboardKey_NumPad8          = 96,
    HidKeyboardKey_NumPad9          = 97,
    HidKeyboardKey_NumPad0          = 98,
    HidKeyboardKey_NumPadDot        = 99,
    HidKeyboardKey_Backslash        = 100,
    HidKeyboardKey_Application      = 101,
    HidKeyboardKey_Power            = 102,
    HidKeyboardKey_NumPadEquals     = 103,
    HidKeyboardKey_F13              = 104,
    HidKeyboardKey_F14              = 105,
    HidKeyboardKey_F15              = 106,
    HidKeyboardKey_F16              = 107,
    HidKeyboardKey_F17              = 108,
    HidKeyboardKey_F18              = 109,
    HidKeyboardKey_F19              = 110,
    HidKeyboardKey_F20              = 111,
    HidKeyboardKey_F21              = 112,
    HidKeyboardKey_F22              = 113,
    HidKeyboardKey_F23              = 114,
    HidKeyboardKey_F24              = 115,
    HidKeyboardKey_NumPadComma      = 133,
    HidKeyboardKey_Ro               = 135,
    HidKeyboardKey_KatakanaHiragana = 136,
    HidKeyboardKey_Yen              = 137,
    HidKeyboardKey_Henkan           = 138,
    HidKeyboardKey_Muhenkan         = 139,
    HidKeyboardKey_NumPadCommaPc98  = 140,
    HidKeyboardKey_HangulEnglish    = 144,
    HidKeyboardKey_Hanja            = 145,
    HidKeyboardKey_Katakana         = 146,
    HidKeyboardKey_Hiragana         = 147,
    HidKeyboardKey_ZenkakuHankaku   = 148,
    HidKeyboardKey_LeftControl      = 224,
    HidKeyboardKey_LeftShift        = 225,
    HidKeyboardKey_LeftAlt          = 226,
    HidKeyboardKey_LeftGui          = 227,
    HidKeyboardKey_RightControl     = 228,
    HidKeyboardKey_RightShift       = 229,
    HidKeyboardKey_RightAlt         = 230,
    HidKeyboardKey_RightGui         = 231,
} HidKeyboardKey;

/// HidKeyboardModifier
typedef enum {
    HidKeyboardModifier_Control       = BIT(0),
    HidKeyboardModifier_Shift         = BIT(1),
    HidKeyboardModifier_LeftAlt       = BIT(2),
    HidKeyboardModifier_RightAlt      = BIT(3),
    HidKeyboardModifier_Gui           = BIT(4),
    HidKeyboardModifier_CapsLock      = BIT(8),
    HidKeyboardModifier_ScrollLock    = BIT(9),
    HidKeyboardModifier_NumLock       = BIT(10),
    HidKeyboardModifier_Katakana      = BIT(11),
    HidKeyboardModifier_Hiragana      = BIT(12),
} HidKeyboardModifier;

/// KeyboardLockKeyEvent
typedef enum {
    HidKeyboardLockKeyEvent_NumLockOn         = BIT(0),         ///< NumLockOn
    HidKeyboardLockKeyEvent_NumLockOff        = BIT(1),         ///< NumLockOff
    HidKeyboardLockKeyEvent_NumLockToggle     = BIT(2),         ///< NumLockToggle
    HidKeyboardLockKeyEvent_CapsLockOn        = BIT(3),         ///< CapsLockOn
    HidKeyboardLockKeyEvent_CapsLockOff       = BIT(4),         ///< CapsLockOff
    HidKeyboardLockKeyEvent_CapsLockToggle    = BIT(5),         ///< CapsLockToggle
    HidKeyboardLockKeyEvent_ScrollLockOn      = BIT(6),         ///< ScrollLockOn
    HidKeyboardLockKeyEvent_ScrollLockOff     = BIT(7),         ///< ScrollLockOff
    HidKeyboardLockKeyEvent_ScrollLockToggle  = BIT(8),         ///< ScrollLockToggle
} HidKeyboardLockKeyEvent;

/// HID controller IDs
typedef enum {
    HidNpadIdType_No1      = 0,    ///< Player 1 controller
    HidNpadIdType_No2      = 1,    ///< Player 2 controller
    HidNpadIdType_No3      = 2,    ///< Player 3 controller
    HidNpadIdType_No4      = 3,    ///< Player 4 controller
    HidNpadIdType_No5      = 4,    ///< Player 5 controller
    HidNpadIdType_No6      = 5,    ///< Player 6 controller
    HidNpadIdType_No7      = 6,    ///< Player 7 controller
    HidNpadIdType_No8      = 7,    ///< Player 8 controller
    HidNpadIdType_Other    = 0x10, ///< Other controller
    HidNpadIdType_Handheld = 0x20, ///< Handheld mode controls
} HidNpadIdType;

/// HID controller styles
typedef enum {
    HidNpadStyleTag_NpadFullKey       = BIT(0),       ///< Pro Controller
    HidNpadStyleTag_NpadHandheld      = BIT(1),       ///< Joy-Con controller in handheld mode
    HidNpadStyleTag_NpadJoyDual       = BIT(2),       ///< Joy-Con controller in dual mode
    HidNpadStyleTag_NpadJoyLeft       = BIT(3),       ///< Joy-Con left controller in single mode
    HidNpadStyleTag_NpadJoyRight      = BIT(4),       ///< Joy-Con right controller in single mode
    HidNpadStyleTag_NpadGc            = BIT(5),       ///< GameCube controller
    HidNpadStyleTag_NpadPalma         = BIT(6),       ///< Poké Ball Plus controller
    HidNpadStyleTag_NpadLark          = BIT(7),       ///< NES/Famicom controller
    HidNpadStyleTag_NpadHandheldLark  = BIT(8),       ///< NES/Famicom controller in handheld mode
    HidNpadStyleTag_NpadLucia         = BIT(9),       ///< SNES controller
    HidNpadStyleTag_NpadLagon         = BIT(10),      ///< N64 controller
    HidNpadStyleTag_NpadLager         = BIT(11),      ///< Sega Genesis controller
    HidNpadStyleTag_NpadSystemExt     = BIT(29),      ///< Generic external controller
    HidNpadStyleTag_NpadSystem        = BIT(30),      ///< Generic controller

    HidNpadStyleSet_NpadFullCtrl = HidNpadStyleTag_NpadFullKey  | HidNpadStyleTag_NpadHandheld | HidNpadStyleTag_NpadJoyDual,  ///< Style set comprising Npad styles containing the full set of controls {FullKey, Handheld, JoyDual}
    HidNpadStyleSet_NpadStandard = HidNpadStyleSet_NpadFullCtrl | HidNpadStyleTag_NpadJoyLeft  | HidNpadStyleTag_NpadJoyRight, ///< Style set comprising all standard Npad styles {FullKey, Handheld, JoyDual, JoyLeft, JoyRight}
} HidNpadStyleTag;

/// HidColorAttribute
typedef enum {
    HidColorAttribute_Ok              = 0,            ///< Ok
    HidColorAttribute_ReadError       = 1,            ///< ReadError
    HidColorAttribute_NoController    = 2,            ///< NoController
} HidColorAttribute;

/// HidNpadButton
typedef enum {
    HidNpadButton_A             = BITL(0),  ///< A button / Right face button
    HidNpadButton_B             = BITL(1),  ///< B button / Down face button
    HidNpadButton_X             = BITL(2),  ///< X button / Up face button
    HidNpadButton_Y             = BITL(3),  ///< Y button / Left face button
    HidNpadButton_StickL        = BITL(4),  ///< Left Stick button
    HidNpadButton_StickR        = BITL(5),  ///< Right Stick button
    HidNpadButton_L             = BITL(6),  ///< L button
    HidNpadButton_R             = BITL(7),  ///< R button
    HidNpadButton_ZL            = BITL(8),  ///< ZL button
    HidNpadButton_ZR            = BITL(9),  ///< ZR button
    HidNpadButton_Plus          = BITL(10), ///< Plus button
    HidNpadButton_Minus         = BITL(11), ///< Minus button
    HidNpadButton_Left          = BITL(12), ///< D-Pad Left button
    HidNpadButton_Up            = BITL(13), ///< D-Pad Up button
    HidNpadButton_Right         = BITL(14), ///< D-Pad Right button
    HidNpadButton_Down          = BITL(15), ///< D-Pad Down button
    HidNpadButton_StickLLeft    = BITL(16), ///< Left Stick pseudo-button when moved Left
    HidNpadButton_StickLUp      = BITL(17), ///< Left Stick pseudo-button when moved Up
    HidNpadButton_StickLRight   = BITL(18), ///< Left Stick pseudo-button when moved Right
    HidNpadButton_StickLDown    = BITL(19), ///< Left Stick pseudo-button when moved Down
    HidNpadButton_StickRLeft    = BITL(20), ///< Right Stick pseudo-button when moved Left
    HidNpadButton_StickRUp      = BITL(21), ///< Right Stick pseudo-button when moved Up
    HidNpadButton_StickRRight   = BITL(22), ///< Right Stick pseudo-button when moved Right
    HidNpadButton_StickRDown    = BITL(23), ///< Right Stick pseudo-button when moved Left
    HidNpadButton_LeftSL        = BITL(24), ///< SL button on Left Joy-Con
    HidNpadButton_LeftSR        = BITL(25), ///< SR button on Left Joy-Con
    HidNpadButton_RightSL       = BITL(26), ///< SL button on Right Joy-Con
    HidNpadButton_RightSR       = BITL(27), ///< SR button on Right Joy-Con
    HidNpadButton_Palma         = BITL(28), ///< Top button on Poké Ball Plus (Palma) controller
    HidNpadButton_Verification  = BITL(29), ///< Verification
    HidNpadButton_HandheldLeftB = BITL(30), ///< B button on Left NES/HVC controller in Handheld mode
    HidNpadButton_LagonCLeft    = BITL(31), ///< Left C button in N64 controller
    HidNpadButton_LagonCUp      = BITL(32), ///< Up C button in N64 controller
    HidNpadButton_LagonCRight   = BITL(33), ///< Right C button in N64 controller
    HidNpadButton_LagonCDown    = BITL(34), ///< Down C button in N64 controller

    HidNpadButton_AnyLeft  = HidNpadButton_Left   | HidNpadButton_StickLLeft  | HidNpadButton_StickRLeft,  ///< Bitmask containing all buttons that are considered Left (D-Pad, Sticks)
    HidNpadButton_AnyUp    = HidNpadButton_Up     | HidNpadButton_StickLUp    | HidNpadButton_StickRUp,    ///< Bitmask containing all buttons that are considered Up (D-Pad, Sticks)
    HidNpadButton_AnyRight = HidNpadButton_Right  | HidNpadButton_StickLRight | HidNpadButton_StickRRight, ///< Bitmask containing all buttons that are considered Right (D-Pad, Sticks)
    HidNpadButton_AnyDown  = HidNpadButton_Down   | HidNpadButton_StickLDown  | HidNpadButton_StickRDown,  ///< Bitmask containing all buttons that are considered Down (D-Pad, Sticks)
    HidNpadButton_AnySL    = HidNpadButton_LeftSL | HidNpadButton_RightSL,                                 ///< Bitmask containing SL buttons on both Joy-Cons (Left/Right)
    HidNpadButton_AnySR    = HidNpadButton_LeftSR | HidNpadButton_RightSR,                                 ///< Bitmask containing SR buttons on both Joy-Cons (Left/Right)
} HidNpadButton;

/// HidDebugPadAttribute
typedef enum {
    HidDebugPadAttribute_IsConnected   = BIT(0),    ///< IsConnected
} HidDebugPadAttribute;

/// HidTouchAttribute
typedef enum {
    HidTouchAttribute_Start            = BIT(0),    ///< Start
    HidTouchAttribute_End              = BIT(1),    ///< End
} HidTouchAttribute;

/// HidMouseAttribute
typedef enum {
    HidMouseAttribute_Transferable     = BIT(0),    ///< Transferable
    HidMouseAttribute_IsConnected      = BIT(1),    ///< IsConnected
} HidMouseAttribute;

/// HidNpadAttribute
typedef enum {
    HidNpadAttribute_IsConnected          = BIT(0),    ///< IsConnected
    HidNpadAttribute_IsWired              = BIT(1),    ///< IsWired
    HidNpadAttribute_IsLeftConnected      = BIT(2),    ///< IsLeftConnected
    HidNpadAttribute_IsLeftWired          = BIT(3),    ///< IsLeftWired
    HidNpadAttribute_IsRightConnected     = BIT(4),    ///< IsRightConnected
    HidNpadAttribute_IsRightWired         = BIT(5),    ///< IsRightWired
} HidNpadAttribute;

/// HidSixAxisSensorAttribute
typedef enum {
    HidSixAxisSensorAttribute_IsConnected     = BIT(0),    ///< IsConnected
    HidSixAxisSensorAttribute_IsInterpolated  = BIT(1),    ///< IsInterpolated
} HidSixAxisSensorAttribute;

/// HidGestureAttribute
typedef enum {
    HidGestureAttribute_IsNewTouch            = BIT(4),    ///< IsNewTouch
    HidGestureAttribute_IsDoubleTap           = BIT(8),    ///< IsDoubleTap
} HidGestureAttribute;

/// HidGestureDirection
typedef enum {
    HidGestureDirection_None           = 0,    ///< None
    HidGestureDirection_Left           = 1,    ///< Left
    HidGestureDirection_Up             = 2,    ///< Up
    HidGestureDirection_Right          = 3,    ///< Right
    HidGestureDirection_Down           = 4,    ///< Down
} HidGestureDirection;

/// HidGestureType
typedef enum {
    HidGestureType_Idle                = 0,    ///< Idle
    HidGestureType_Complete            = 1,    ///< Complete
    HidGestureType_Cancel              = 2,    ///< Cancel
    HidGestureType_Touch               = 3,    ///< Touch
    HidGestureType_Press               = 4,    ///< Press
    HidGestureType_Tap                 = 5,    ///< Tap
    HidGestureType_Pan                 = 6,    ///< Pan
    HidGestureType_Swipe               = 7,    ///< Swipe
    HidGestureType_Pinch               = 8,    ///< Pinch
    HidGestureType_Rotate              = 9,    ///< Rotate
} HidGestureType;

/// GyroscopeZeroDriftMode
typedef enum {
    HidGyroscopeZeroDriftMode_Loose    = 0,   ///< Loose
    HidGyroscopeZeroDriftMode_Standard = 1,   ///< Standard
    HidGyroscopeZeroDriftMode_Tight    = 2,   ///< Tight
} HidGyroscopeZeroDriftMode;

/// NpadJoyHoldType
typedef enum {
    HidNpadJoyHoldType_Vertical          = 0,       ///< Default / Joy-Con held vertically.
    HidNpadJoyHoldType_Horizontal        = 1,       ///< Joy-Con held horizontally.
} HidNpadJoyHoldType;

/// NpadJoyDeviceType
typedef enum {
    HidNpadJoyDeviceType_Left            = 0,       ///< Left
    HidNpadJoyDeviceType_Right           = 1,       ///< Right
} HidNpadJoyDeviceType;

/// This controls how many Joy-Cons must be attached for handheld-mode to be activated.
typedef enum {
    HidNpadHandheldActivationMode_Dual     = 0,     ///< Dual (2 Joy-Cons)
    HidNpadHandheldActivationMode_Single   = 1,     ///< Single (1 Joy-Con)
    HidNpadHandheldActivationMode_None     = 2,     ///< None (0 Joy-Cons)
} HidNpadHandheldActivationMode;

/// NpadJoyAssignmentMode
typedef enum {
    HidNpadJoyAssignmentMode_Dual   = 0,            ///< Dual (Set by \ref hidSetNpadJoyAssignmentModeDual)
    HidNpadJoyAssignmentMode_Single = 1,            ///< Single (Set by hidSetNpadJoyAssignmentModeSingle*())
} HidNpadJoyAssignmentMode;

/// NpadCommunicationMode
typedef enum {
    HidNpadCommunicationMode_5ms       = 0,         ///< 5ms
    HidNpadCommunicationMode_10ms      = 1,         ///< 10ms
    HidNpadCommunicationMode_15ms      = 2,         ///< 15ms
    HidNpadCommunicationMode_Default   = 3,         ///< Default
} HidNpadCommunicationMode;

/// DeviceType (system)
typedef enum {
    HidDeviceTypeBits_FullKey               = BIT(0),  ///< Pro Controller and Gc controller.
    HidDeviceTypeBits_DebugPad              = BIT(1),  ///< DebugPad
    HidDeviceTypeBits_HandheldLeft          = BIT(2),  ///< Joy-Con/Famicom/NES left controller in handheld mode.
    HidDeviceTypeBits_HandheldRight         = BIT(3),  ///< Joy-Con/Famicom/NES right controller in handheld mode.
    HidDeviceTypeBits_JoyLeft               = BIT(4),  ///< Joy-Con left controller.
    HidDeviceTypeBits_JoyRight              = BIT(5),  ///< Joy-Con right controller.
    HidDeviceTypeBits_Palma                 = BIT(6),  ///< Poké Ball Plus controller.
    HidDeviceTypeBits_LarkHvcLeft           = BIT(7),  ///< Famicom left controller.
    HidDeviceTypeBits_LarkHvcRight          = BIT(8),  ///< Famicom right controller (with microphone).
    HidDeviceTypeBits_LarkNesLeft           = BIT(9),  ///< NES left controller.
    HidDeviceTypeBits_LarkNesRight          = BIT(10), ///< NES right controller.
    HidDeviceTypeBits_HandheldLarkHvcLeft   = BIT(11), ///< Famicom left controller in handheld mode.
    HidDeviceTypeBits_HandheldLarkHvcRight  = BIT(12), ///< Famicom right controller (with microphone) in handheld mode.
    HidDeviceTypeBits_HandheldLarkNesLeft   = BIT(13), ///< NES left controller in handheld mode.
    HidDeviceTypeBits_HandheldLarkNesRight  = BIT(14), ///< NES right controller in handheld mode.
    HidDeviceTypeBits_Lucia                 = BIT(15), ///< SNES controller
    HidDeviceTypeBits_Lagon                 = BIT(16), ///< N64 controller
    HidDeviceTypeBits_Lager                 = BIT(17), ///< Sega Genesis controller
    HidDeviceTypeBits_System                = BIT(31), ///< Generic controller.
} HidDeviceTypeBits;

/// Internal DeviceType for [9.0.0+]. Converted to/from the pre-9.0.0 version of this by the hiddbg funcs.
typedef enum {
    HidDeviceType_JoyRight1       = 1,   ///< ::HidDeviceTypeBits_JoyRight
    HidDeviceType_JoyLeft2        = 2,   ///< ::HidDeviceTypeBits_JoyLeft
    HidDeviceType_FullKey3        = 3,   ///< ::HidDeviceTypeBits_FullKey
    HidDeviceType_JoyLeft4        = 4,   ///< ::HidDeviceTypeBits_JoyLeft
    HidDeviceType_JoyRight5       = 5,   ///< ::HidDeviceTypeBits_JoyRight
    HidDeviceType_FullKey6        = 6,   ///< ::HidDeviceTypeBits_FullKey
    HidDeviceType_LarkHvcLeft     = 7,   ///< ::HidDeviceTypeBits_LarkHvcLeft, ::HidDeviceTypeBits_HandheldLarkHvcLeft
    HidDeviceType_LarkHvcRight    = 8,   ///< ::HidDeviceTypeBits_LarkHvcRight, ::HidDeviceTypeBits_HandheldLarkHvcRight
    HidDeviceType_LarkNesLeft     = 9,   ///< ::HidDeviceTypeBits_LarkNesLeft, ::HidDeviceTypeBits_HandheldLarkNesLeft
    HidDeviceType_LarkNesRight    = 10,  ///< ::HidDeviceTypeBits_LarkNesRight, ::HidDeviceTypeBits_HandheldLarkNesRight
    HidDeviceType_Lucia           = 11,  ///< ::HidDeviceTypeBits_Lucia
    HidDeviceType_Palma           = 12,  ///< [9.0.0+] ::HidDeviceTypeBits_Palma
    HidDeviceType_FullKey13       = 13,  ///< ::HidDeviceTypeBits_FullKey
    HidDeviceType_FullKey15       = 15,  ///< ::HidDeviceTypeBits_FullKey
    HidDeviceType_DebugPad        = 17,  ///< ::HidDeviceTypeBits_DebugPad
    HidDeviceType_System19        = 19,  ///< ::HidDeviceTypeBits_System with \ref HidNpadStyleTag |= ::HidNpadStyleTag_NpadFullKey.
    HidDeviceType_System20        = 20,  ///< ::HidDeviceTypeBits_System with \ref HidNpadStyleTag |= ::HidNpadStyleTag_NpadJoyDual.
    HidDeviceType_System21        = 21,  ///< ::HidDeviceTypeBits_System with \ref HidNpadStyleTag |= ::HidNpadStyleTag_NpadJoyDual.
    HidDeviceType_Lagon           = 22,  ///< ::HidDeviceTypeBits_Lagon
    HidDeviceType_Lager           = 28,  ///< ::HidDeviceTypeBits_Lager
} HidDeviceType;

/// AppletFooterUiType (system)
typedef enum {
    HidAppletFooterUiType_None                             = 0,     ///< None
    HidAppletFooterUiType_HandheldNone                     = 1,     ///< HandheldNone
    HidAppletFooterUiType_HandheldJoyConLeftOnly           = 2,     ///< HandheldJoyConLeftOnly
    HidAppletFooterUiType_HandheldJoyConRightOnly          = 3,     ///< HandheldJoyConRightOnly
    HidAppletFooterUiType_HandheldJoyConLeftJoyConRight    = 4,     ///< HandheldJoyConLeftJoyConRight
    HidAppletFooterUiType_JoyDual                          = 5,     ///< JoyDual
    HidAppletFooterUiType_JoyDualLeftOnly                  = 6,     ///< JoyDualLeftOnly
    HidAppletFooterUiType_JoyDualRightOnly                 = 7,     ///< JoyDualRightOnly
    HidAppletFooterUiType_JoyLeftHorizontal                = 8,     ///< JoyLeftHorizontal
    HidAppletFooterUiType_JoyLeftVertical                  = 9,     ///< JoyLeftVertical
    HidAppletFooterUiType_JoyRightHorizontal               = 10,    ///< JoyRightHorizontal
    HidAppletFooterUiType_JoyRightVertical                 = 11,    ///< JoyRightVertical
    HidAppletFooterUiType_SwitchProController              = 12,    ///< SwitchProController
    HidAppletFooterUiType_CompatibleProController          = 13,    ///< CompatibleProController
    HidAppletFooterUiType_CompatibleJoyCon                 = 14,    ///< CompatibleJoyCon
    HidAppletFooterUiType_LarkHvc1                         = 15,    ///< LarkHvc1
    HidAppletFooterUiType_LarkHvc2                         = 16,    ///< LarkHvc2
    HidAppletFooterUiType_LarkNesLeft                      = 17,    ///< LarkNesLeft
    HidAppletFooterUiType_LarkNesRight                     = 18,    ///< LarkNesRight
    HidAppletFooterUiType_Lucia                            = 19,    ///< Lucia
    HidAppletFooterUiType_Verification                     = 20,    ///< Verification
    HidAppletFooterUiType_Lagon                            = 21,    ///< [13.0.0+] Lagon
} HidAppletFooterUiType;

/// NpadInterfaceType (system)
typedef enum {
    HidNpadInterfaceType_Bluetooth = 1,    ///< Bluetooth.
    HidNpadInterfaceType_Rail      = 2,    ///< Rail.
    HidNpadInterfaceType_USB       = 3,    ///< USB.
    HidNpadInterfaceType_Unknown4  = 4,    ///< Unknown.
} HidNpadInterfaceType;

/// XcdInterfaceType
typedef enum {
    XcdInterfaceType_Bluetooth  = BIT(0),
    XcdInterfaceType_Uart       = BIT(1),
    XcdInterfaceType_Usb        = BIT(2),
    XcdInterfaceType_FieldSet   = BIT(7),
} XcdInterfaceType;

/// NpadLarkType
typedef enum {
    HidNpadLarkType_Invalid     = 0,    ///< Invalid
    HidNpadLarkType_H1          = 1,    ///< H1
    HidNpadLarkType_H2          = 2,    ///< H2
    HidNpadLarkType_NL          = 3,    ///< NL
    HidNpadLarkType_NR          = 4,    ///< NR
} HidNpadLarkType;

/// NpadLuciaType
typedef enum {
    HidNpadLuciaType_Invalid    = 0,    ///< Invalid
    HidNpadLuciaType_J          = 1,    ///< J
    HidNpadLuciaType_E          = 2,    ///< E
    HidNpadLuciaType_U          = 3,    ///< U
} HidNpadLuciaType;

/// NpadLagerType
typedef enum {
    HidNpadLagerType_Invalid    = 0,    ///< Invalid
    HidNpadLagerType_J          = 1,    ///< J
    HidNpadLagerType_E          = 2,    ///< E
    HidNpadLagerType_U          = 3,    ///< U
} HidNpadLagerType;

/// Type values for HidVibrationDeviceInfo::type.
typedef enum {
    HidVibrationDeviceType_Unknown                          = 0,     ///< Unknown
    HidVibrationDeviceType_LinearResonantActuator           = 1,     ///< LinearResonantActuator
    HidVibrationDeviceType_GcErm                            = 2,     ///< GcErm (::HidNpadStyleTag_NpadGc)
} HidVibrationDeviceType;

/// VibrationDevicePosition
typedef enum {
    HidVibrationDevicePosition_None                         = 0,     ///< None
    HidVibrationDevicePosition_Left                         = 1,     ///< Left
    HidVibrationDevicePosition_Right                        = 2,     ///< Right
} HidVibrationDevicePosition;

/// VibrationGcErmCommand
typedef enum {
    HidVibrationGcErmCommand_Stop                           = 0,     ///< Stops the vibration with a decay phase.
    HidVibrationGcErmCommand_Start                          = 1,     ///< Starts the vibration.
    HidVibrationGcErmCommand_StopHard                       = 2,     ///< Stops the vibration immediately, with no decay phase.
} HidVibrationGcErmCommand;

/// PalmaOperationType
typedef enum {
    HidPalmaOperationType_PlayActivity                                 = 0,     ///< PlayActivity
    HidPalmaOperationType_SetFrModeType                                = 1,     ///< SetFrModeType
    HidPalmaOperationType_ReadStep                                     = 2,     ///< ReadStep
    HidPalmaOperationType_EnableStep                                   = 3,     ///< EnableStep
    HidPalmaOperationType_ResetStep                                    = 4,     ///< ResetStep
    HidPalmaOperationType_ReadApplicationSection                       = 5,     ///< ReadApplicationSection
    HidPalmaOperationType_WriteApplicationSection                      = 6,     ///< WriteApplicationSection
    HidPalmaOperationType_ReadUniqueCode                               = 7,     ///< ReadUniqueCode
    HidPalmaOperationType_SetUniqueCodeInvalid                         = 8,     ///< SetUniqueCodeInvalid
    HidPalmaOperationType_WriteActivityEntry                           = 9,     ///< WriteActivityEntry
    HidPalmaOperationType_WriteRgbLedPatternEntry                      = 10,    ///< WriteRgbLedPatternEntry
    HidPalmaOperationType_WriteWaveEntry                               = 11,    ///< WriteWaveEntry
    HidPalmaOperationType_ReadDataBaseIdentificationVersion            = 12,    ///< ReadDataBaseIdentificationVersion
    HidPalmaOperationType_WriteDataBaseIdentificationVersion           = 13,    ///< WriteDataBaseIdentificationVersion
    HidPalmaOperationType_SuspendFeature                               = 14,    ///< SuspendFeature
    HidPalmaOperationType_ReadPlayLog                                  = 15,    ///< [5.1.0+] ReadPlayLog
    HidPalmaOperationType_ResetPlayLog                                 = 16,    ///< [5.1.0+] ResetPlayLog
} HidPalmaOperationType;

/// PalmaFrModeType
typedef enum {
    HidPalmaFrModeType_Off                                  = 0,     ///< Off
    HidPalmaFrModeType_B01                                  = 1,     ///< B01
    HidPalmaFrModeType_B02                                  = 2,     ///< B02
    HidPalmaFrModeType_B03                                  = 3,     ///< B03
    HidPalmaFrModeType_Downloaded                           = 4,     ///< Downloaded
} HidPalmaFrModeType;

/// PalmaWaveSet
typedef enum {
    HidPalmaWaveSet_Small                                   = 0,     ///< Small
    HidPalmaWaveSet_Medium                                  = 1,     ///< Medium
    HidPalmaWaveSet_Large                                   = 2,     ///< Large
} HidPalmaWaveSet;

/// PalmaFeature
typedef enum {
    HidPalmaFeature_FrMode                                  = BIT(0),     ///< FrMode
    HidPalmaFeature_RumbleFeedback                          = BIT(1),     ///< RumbleFeedback
    HidPalmaFeature_Step                                    = BIT(2),     ///< Step
    HidPalmaFeature_MuteSwitch                              = BIT(3),     ///< MuteSwitch
} HidPalmaFeature;

/// HidAnalogStickState
typedef struct HidAnalogStickState {
    s32 x;                                    ///< X
    s32 y;                                    ///< Y
} HidAnalogStickState;

/// HidVector
typedef struct HidVector {
    float x;
    float y;
    float z;
} HidVector;

/// HidDirectionState
typedef struct HidDirectionState {
    float direction[3][3];                      ///< 3x3 matrix
} HidDirectionState;

#define JOYSTICK_MAX (0x7FFF)
#define JOYSTICK_MIN (-0x7FFF)

// End enums and output structs

/// HidCommonLifoHeader
typedef struct HidCommonLifoHeader {
    u64 unused;                                 ///< Unused
    u64 buffer_count;                           ///< BufferCount
    u64 tail;                                   ///< Tail
    u64 count;                                  ///< Count
} HidCommonLifoHeader;

// Begin HidDebugPad

/// HidDebugPadState
typedef struct HidDebugPadState {
    u64 sampling_number;                        ///< SamplingNumber
    u32 attributes;                             ///< Bitfield of \ref HidDebugPadAttribute.
    u32 buttons;                                ///< Bitfield of \ref HidDebugPadButton.
    HidAnalogStickState analog_stick_r;         ///< AnalogStickR
    HidAnalogStickState analog_stick_l;         ///< AnalogStickL
} HidDebugPadState;

/// HidDebugPadStateAtomicStorage
typedef struct HidDebugPadStateAtomicStorage {
    u64 sampling_number;                        ///< SamplingNumber
    HidDebugPadState state;                     ///< \ref HidDebugPadState
} HidDebugPadStateAtomicStorage;

/// HidDebugPadLifo
typedef struct HidDebugPadLifo {
    HidCommonLifoHeader header;                 ///< \ref HidCommonLifoHeader
    HidDebugPadStateAtomicStorage storage[17];  ///< \ref HidDebugPadStateAtomicStorage
} HidDebugPadLifo;

/// HidDebugPadSharedMemoryFormat
typedef struct HidDebugPadSharedMemoryFormat {
    HidDebugPadLifo lifo;
    u8 padding[0x138];
} HidDebugPadSharedMemoryFormat;

// End HidDebugPad

// Begin HidTouchScreen

/// HidTouchState
typedef struct HidTouchState {
    u64 delta_time;                             ///< DeltaTime
    u32 attributes;                             ///< Bitfield of \ref HidTouchAttribute.
    u32 finger_id;                              ///< FingerId
    u32 x;                                      ///< X
    u32 y;                                      ///< Y
    u32 diameter_x;                             ///< DiameterX
    u32 diameter_y;                             ///< DiameterY
    u32 rotation_angle;                         ///< RotationAngle
    u32 reserved;                               ///< Reserved
} HidTouchState;

/// HidTouchScreenState
typedef struct HidTouchScreenState {
    u64 sampling_number;                        ///< SamplingNumber
    s32 count;                                  ///< Number of entries in the touches array.
    u32 reserved;                               ///< Reserved
    HidTouchState touches[16];                  ///< Array of \ref HidTouchState, with the above count.
} HidTouchScreenState;

/// HidTouchScreenStateAtomicStorage
typedef struct HidTouchScreenStateAtomicStorage {
    u64 sampling_number;                             ///< SamplingNumber
    HidTouchScreenState state;                       ///< \ref HidTouchScreenState
} HidTouchScreenStateAtomicStorage;

/// HidTouchScreenLifo
typedef struct HidTouchScreenLifo {
    HidCommonLifoHeader header;                      ///< \ref HidCommonLifoHeader
    HidTouchScreenStateAtomicStorage storage[17];    ///< \ref HidTouchScreenStateAtomicStorage
} HidTouchScreenLifo;

/// HidTouchScreenSharedMemoryFormat
typedef struct HidTouchScreenSharedMemoryFormat {
    HidTouchScreenLifo lifo;
    u8 padding[0x3c8];
} HidTouchScreenSharedMemoryFormat;

/// HidTouchScreenConfigurationForNx
typedef struct {
    u8 mode;                                         ///< \ref HidTouchScreenModeForNx
    u8 reserved[0xF];                                ///< Reserved
} HidTouchScreenConfigurationForNx;

// End HidTouchScreen

// Begin HidMouse

/// HidMouseState
typedef struct HidMouseState {
    u64 sampling_number;                        ///< SamplingNumber
    s32 x;                                      ///< X
    s32 y;                                      ///< Y
    s32 delta_x;                                ///< DeltaX
    s32 delta_y;                                ///< DeltaY
    s32 wheel_delta_x;                          ///< WheelDeltaX
    s32 wheel_delta_y;                          ///< WheelDeltaY
    u32 buttons;                                ///< Bitfield of \ref HidMouseButton.
    u32 attributes;                             ///< Bitfield of \ref HidMouseAttribute.
} HidMouseState;

/// HidMouseStateAtomicStorage
typedef struct HidMouseStateAtomicStorage {
    u64 sampling_number;                        ///< SamplingNumber
    HidMouseState state;
} HidMouseStateAtomicStorage;

/// HidMouseLifo
typedef struct HidMouseLifo {
    HidCommonLifoHeader header;
    HidMouseStateAtomicStorage storage[17];
} HidMouseLifo;

/// HidMouseSharedMemoryFormat
typedef struct HidMouseSharedMemoryFormat {
    HidMouseLifo lifo;
    u8 padding[0xB0];
} HidMouseSharedMemoryFormat;

// End HidMouse

// Begin HidKeyboard

/// HidKeyboardState
typedef struct HidKeyboardState {
    u64 sampling_number;                        ///< SamplingNumber
    u64 modifiers;                              ///< Bitfield of \ref HidKeyboardModifier.
    u64 keys[4];
} HidKeyboardState;

/// HidKeyboardStateAtomicStorage
typedef struct HidKeyboardStateAtomicStorage {
    u64 sampling_number;                        ///< SamplingNumber
    HidKeyboardState state;
} HidKeyboardStateAtomicStorage;

/// HidKeyboardLifo
typedef struct HidKeyboardLifo {
    HidCommonLifoHeader header;
    HidKeyboardStateAtomicStorage storage[17];
} HidKeyboardLifo;

/// HidKeyboardSharedMemoryFormat
typedef struct HidKeyboardSharedMemoryFormat {
    HidKeyboardLifo lifo;
    u8 padding[0x28];
} HidKeyboardSharedMemoryFormat;

// End HidKeyboard

// Begin HidBasicXpad

/// HidBasicXpadState
typedef struct {
    u64 sampling_number;
    u32 attributes;
    u32 buttons;
    u64 analog_stick_left;
    u64 analog_stick_right;
} HidBasicXpadState;

/// HidBasicXpadStateAtomicStorage
typedef struct {
    u64 sampling_number;
    HidBasicXpadState state;
} HidBasicXpadStateAtomicStorage;

/// HidBasicXpadLifo
typedef struct {
    HidCommonLifoHeader header;
    HidBasicXpadStateAtomicStorage storage[17];
} HidBasicXpadLifo;

/// HidBasicXpadSharedMemoryEntry
typedef struct {
    HidBasicXpadLifo lifo;
    u8 padding[0x138];
} HidBasicXpadSharedMemoryEntry;

/// HidBasicXpadSharedMemoryFormat
typedef struct {
    HidBasicXpadSharedMemoryEntry entries[4];
} HidBasicXpadSharedMemoryFormat;

// End HidBasicXpad

// Begin HidDigitizer

/// HidDigitizerState
typedef struct {
    u64 sampling_number;
    u32 unk_0x8;
    u32 unk_0xC;
    u32 attributes;
    u32 buttons;
    u32 unk_0x18;
    u32 unk_0x1C;
    u32 unk_0x20;
    u32 unk_0x24;
    u32 unk_0x28;
    u32 unk_0x2C;
    u32 unk_0x30;
    u32 unk_0x34;
    u32 unk_0x38;
    u32 unk_0x3C;
    u32 unk_0x40;
    u32 unk_0x44;
    u32 unk_0x48;
    u32 unk_0x4C;
    u32 unk_0x50;
    u32 unk_0x54;
} HidDigitizerState;

/// HidDigitizerStateAtomicStorage
typedef struct {
    u64 sampling_number;
    HidDigitizerState state;
} HidDigitizerStateAtomicStorage;

/// HidDigitizerLifo
typedef struct {
    HidCommonLifoHeader header;
    HidDigitizerStateAtomicStorage storage[17];
} HidDigitizerLifo;

/// HidDigitizerSharedMemoryFormat
typedef struct {
    HidDigitizerLifo lifo;
    u8 padding[0x980];
} HidDigitizerSharedMemoryFormat;

// End HidDigitizer

// Begin HidHomeButton

/// HidHomeButtonState
typedef struct {
    u64 sampling_number;
    u64 buttons;
} HidHomeButtonState;

/// HidHomeButtonStateAtomicStorage
typedef struct {
    u64 sampling_number;
    HidHomeButtonState state;
} HidHomeButtonStateAtomicStorage;

/// HidHomeButtonLifo
typedef struct {
    HidCommonLifoHeader header;
    HidHomeButtonStateAtomicStorage storage[17];
} HidHomeButtonLifo;

/// HidHomeButtonSharedMemoryFormat
typedef struct {
    HidHomeButtonLifo lifo;
    u8 padding[0x48];
} HidHomeButtonSharedMemoryFormat;

// End HidHomeButton

// Begin HidSleepButton

/// HidSleepButtonState
typedef struct {
    u64 sampling_number;
    u64 buttons;
} HidSleepButtonState;

/// HidSleepButtonStateAtomicStorage
typedef struct {
    u64 sampling_number;
    HidSleepButtonState state;
} HidSleepButtonStateAtomicStorage;

/// HidSleepButtonLifo
typedef struct {
    HidCommonLifoHeader header;
    HidSleepButtonStateAtomicStorage storage[17];
} HidSleepButtonLifo;

/// HidSleepButtonSharedMemoryFormat
typedef struct {
    HidSleepButtonLifo lifo;
    u8 padding[0x48];
} HidSleepButtonSharedMemoryFormat;

// End HidSleepButton

// Begin HidCaptureButton

/// HidCaptureButtonState
typedef struct {
    u64 sampling_number;
    u64 buttons;
} HidCaptureButtonState;

/// HidCaptureButtonStateAtomicStorage
typedef struct {
    u64 sampling_number;
    HidCaptureButtonState state;
} HidCaptureButtonStateAtomicStorage;

/// HidCaptureButtonLifo
typedef struct {
    HidCommonLifoHeader header;
    HidCaptureButtonStateAtomicStorage storage[17];
} HidCaptureButtonLifo;

/// HidCaptureButtonSharedMemoryFormat
typedef struct {
    HidCaptureButtonLifo lifo;
    u8 padding[0x48];
} HidCaptureButtonSharedMemoryFormat;

// End HidCaptureButton

// Begin HidInputDetector

/// HidInputDetectorState
typedef struct {
    u64 input_source_state;
    u64 sampling_number;
} HidInputDetectorState;

/// HidInputDetectorStateAtomicStorage
typedef struct {
    u64 sampling_number;
    HidInputDetectorState state;
} HidInputDetectorStateAtomicStorage;

/// HidInputDetectorLifo
typedef struct {
    HidCommonLifoHeader header;
    HidInputDetectorStateAtomicStorage storage[2];
} HidInputDetectorLifo;

/// HidInputDetectorSharedMemoryEntry
typedef struct {
    HidInputDetectorLifo lifo;
    u8 padding[0x30];
} HidInputDetectorSharedMemoryEntry;

/// HidInputDetectorSharedMemoryFormat
typedef struct {
    HidInputDetectorSharedMemoryEntry entries[16];
} HidInputDetectorSharedMemoryFormat;

// End HidInputDetector

// Begin HidUniquePad

/// HidUniquePadConfigMutex
typedef struct {
    u8 unk_0x0[0x20];
} HidUniquePadConfigMutex;

/// HidSixAxisSensorUserCalibrationState
typedef struct {
    u32 flags;
    u8 reserved[4];
    u64 stage;
    u64 sampling_number;
} HidSixAxisSensorUserCalibrationState;

/// HidSixAxisSensorUserCalibrationStateAtomicStorage
typedef struct {
    u64 sampling_number;
    HidSixAxisSensorUserCalibrationState calib_state;
} HidSixAxisSensorUserCalibrationStateAtomicStorage;

/// HidSixAxisSensorUserCalibrationStateLifo
typedef struct {
    HidCommonLifoHeader header;
    HidSixAxisSensorUserCalibrationStateAtomicStorage storage[2];
} HidSixAxisSensorUserCalibrationStateLifo;

/// HidAnalogStickCalibrationStateImpl
typedef struct {
    u64 state;
    u64 flags;
    u64 stage;
    u64 sampling_number;
} HidAnalogStickCalibrationStateImpl;

/// HidAnalogStickCalibrationStateImplAtomicStorage
typedef struct {
    u64 sampling_number;
    HidAnalogStickCalibrationStateImpl calib_state;
} HidAnalogStickCalibrationStateImplAtomicStorage;

/// HidAnalogStickCalibrationStateImplLifo
typedef struct {
    HidCommonLifoHeader header;
    HidAnalogStickCalibrationStateImplAtomicStorage storage[2];
} HidAnalogStickCalibrationStateImplLifo;

/// HidUniquePadConfig
typedef struct {
    u32 type;
    u32 interface;
    u8 serial_number[0x10];
    u32 controller_number;
    bool is_active;
    u8 reserved[3];
    u64 sampling_number;
} HidUniquePadConfig;

/// HidUniquePadConfigAtomicStorage
typedef struct {
    u64 sampling_number;
    HidUniquePadConfig config;
} HidUniquePadConfigAtomicStorage;

/// HidUniquePadConfigLifo
typedef struct {
    HidCommonLifoHeader header;
    HidUniquePadConfigAtomicStorage storage[2];
} HidUniquePadConfigLifo;

/// HidUniquePadLifo
typedef struct {
    HidUniquePadConfigLifo config_lifo;
    HidAnalogStickCalibrationStateImplLifo analog_stick_calib_lifo[2];
    HidSixAxisSensorUserCalibrationStateLifo sixaxis_calib_lifo;
    HidUniquePadConfigMutex mutex;
} HidUniquePadLifo;

/// HidUniquePadSharedMemoryEntry
typedef struct {
    HidUniquePadLifo lifo;
    u8 padding[0x220];
} HidUniquePadSharedMemoryEntry;

/// HidUniquePadSharedMemoryFormat
typedef struct {
    HidUniquePadSharedMemoryEntry entries[16];
} HidUniquePadSharedMemoryFormat;

// End HidUniquePad

// Begin HidNpad

/// Npad colors.
/// Color fields are zero when not set.
typedef struct HidNpadControllerColor {
    u32 main;    ///< RGBA Body Color
    u32 sub;     ///< RGBA Buttons Color
} HidNpadControllerColor;

/// HidNpadFullKeyColorState
typedef struct HidNpadFullKeyColorState {
    u32 attribute;                               ///< \ref HidColorAttribute
    HidNpadControllerColor full_key;             ///< \ref HidNpadControllerColor FullKey
} HidNpadFullKeyColorState;

/// HidNpadJoyColorState
typedef struct HidNpadJoyColorState {
    u32 attribute;                               ///< \ref HidColorAttribute
    HidNpadControllerColor left;                 ///< \ref HidNpadControllerColor Left
    HidNpadControllerColor right;                ///< \ref HidNpadControllerColor Right
} HidNpadJoyColorState;

/// HidNpadCommonState
typedef struct HidNpadCommonState {
    u64 sampling_number;                                ///< SamplingNumber
    u64 buttons;                                        ///< Bitfield of \ref HidNpadButton.
    HidAnalogStickState analog_stick_l;                 ///< AnalogStickL
    HidAnalogStickState analog_stick_r;                 ///< AnalogStickR
    u32 attributes;                                     ///< Bitfield of \ref HidNpadAttribute.
    u32 reserved;                                       ///< Reserved
} HidNpadCommonState;

typedef HidNpadCommonState HidNpadFullKeyState;         ///< State for ::HidNpadStyleTag_NpadFullKey.
typedef HidNpadCommonState HidNpadHandheldState;        ///< State for ::HidNpadStyleTag_NpadHandheld.
typedef HidNpadCommonState HidNpadJoyDualState;         ///< State for ::HidNpadStyleTag_NpadJoyDual.
typedef HidNpadCommonState HidNpadJoyLeftState;         ///< State for ::HidNpadStyleTag_NpadJoyLeft.
typedef HidNpadCommonState HidNpadJoyRightState;        ///< State for ::HidNpadStyleTag_NpadJoyRight.

/// State for ::HidNpadStyleTag_NpadGc. Loaded from the same lifo as \ref HidNpadFullKeyState, with the additional trigger_l/trigger_r loaded from elsewhere.
typedef struct HidNpadGcState {
    u64 sampling_number;                                ///< SamplingNumber
    u64 buttons;                                        ///< Bitfield of \ref HidNpadButton.
    HidAnalogStickState analog_stick_l;                 ///< AnalogStickL
    HidAnalogStickState analog_stick_r;                 ///< AnalogStickR
    u32 attributes;                                     ///< Bitfield of \ref HidNpadAttribute.
    u32 trigger_l;                                      ///< L analog trigger. Valid range: 0x0-0x7FFF.
    u32 trigger_r;                                      ///< R analog trigger. Valid range: 0x0-0x7FFF.
    u32 pad;
} HidNpadGcState;

typedef HidNpadCommonState HidNpadPalmaState;           ///< State for ::HidNpadStyleTag_NpadPalma.

/// State for ::HidNpadStyleTag_NpadLark. The base state is loaded from the same lifo as \ref HidNpadFullKeyState.
typedef struct HidNpadLarkState {
    u64 sampling_number;                                ///< SamplingNumber
    u64 buttons;                                        ///< Bitfield of \ref HidNpadButton.
    HidAnalogStickState analog_stick_l;                 ///< This is always zero.
    HidAnalogStickState analog_stick_r;                 ///< This is always zero.
    u32 attributes;                                     ///< Bitfield of \ref HidNpadAttribute.
    HidNpadLarkType lark_type_l_and_main;               ///< \ref HidNpadLarkType LarkTypeLAndMain
} HidNpadLarkState;

/// State for ::HidNpadStyleTag_NpadHandheldLark. The base state is loaded from the same lifo as \ref HidNpadHandheldState.
typedef struct HidNpadHandheldLarkState {
    u64 sampling_number;                                ///< SamplingNumber
    u64 buttons;                                        ///< Bitfield of \ref HidNpadButton.
    HidAnalogStickState analog_stick_l;                 ///< AnalogStickL
    HidAnalogStickState analog_stick_r;                 ///< AnalogStickR
    u32 attributes;                                     ///< Bitfield of \ref HidNpadAttribute.
    HidNpadLarkType lark_type_l_and_main;               ///< \ref HidNpadLarkType LarkTypeLAndMain
    HidNpadLarkType lark_type_r;                        ///< \ref HidNpadLarkType LarkTypeR
    u32 pad;
} HidNpadHandheldLarkState;

/// State for ::HidNpadStyleTag_NpadLucia. The base state is loaded from the same lifo as \ref HidNpadFullKeyState.
typedef struct HidNpadLuciaState {
    u64 sampling_number;                                ///< SamplingNumber
    u64 buttons;                                        ///< Bitfield of \ref HidNpadButton.
    HidAnalogStickState analog_stick_l;                 ///< This is always zero.
    HidAnalogStickState analog_stick_r;                 ///< This is always zero.
    u32 attributes;                                     ///< Bitfield of \ref HidNpadAttribute.
    HidNpadLuciaType lucia_type;                        ///< \ref HidNpadLuciaType
} HidNpadLuciaState;

typedef HidNpadCommonState HidNpadLagerState;           ///< State for ::HidNpadStyleTag_NpadLager. Analog-sticks state are always zero.

typedef HidNpadCommonState HidNpadSystemExtState;       ///< State for ::HidNpadStyleTag_NpadSystemExt.
typedef HidNpadCommonState HidNpadSystemState;          ///< State for ::HidNpadStyleTag_NpadSystem. Analog-sticks state are always zero. Only the following button bits are available: HidNpadButton_A, HidNpadButton_B, HidNpadButton_X, HidNpadButton_Y, HidNpadButton_Left, HidNpadButton_Up, HidNpadButton_Right, HidNpadButton_Down, HidNpadButton_L, HidNpadButton_R.

/// HidNpadCommonStateAtomicStorage
typedef struct HidNpadCommonStateAtomicStorage {
    u64 sampling_number;                                ///< SamplingNumber
    HidNpadCommonState state;
} HidNpadCommonStateAtomicStorage;

/// HidNpadCommonLifo
typedef struct HidNpadCommonLifo {
    HidCommonLifoHeader header;
    HidNpadCommonStateAtomicStorage storage[17];
} HidNpadCommonLifo;

/// HidNpadGcTriggerState
typedef struct HidNpadGcTriggerState {
    u64 sampling_number;                                ///< SamplingNumber
    u32 trigger_l;
    u32 trigger_r;
} HidNpadGcTriggerState;

/// HidNpadGcTriggerStateAtomicStorage
typedef struct HidNpadGcTriggerStateAtomicStorage {
    u64 sampling_number;                                ///< SamplingNumber
    HidNpadGcTriggerState state;
} HidNpadGcTriggerStateAtomicStorage;

/// HidNpadGcTriggerLifo
typedef struct HidNpadGcTriggerLifo {
    HidCommonLifoHeader header;
    HidNpadGcTriggerStateAtomicStorage storage[17];
} HidNpadGcTriggerLifo;

/// HidSixAxisSensorState
typedef struct HidSixAxisSensorState {
    u64 delta_time;                                     ///< DeltaTime
    u64 sampling_number;                                ///< SamplingNumber
    HidVector acceleration;                             ///< Acceleration
    HidVector angular_velocity;                         ///< AngularVelocity
    HidVector angle;                                    ///< Angle
    HidDirectionState direction;                        ///< Direction
    u32 attributes;                                     ///< Bitfield of \ref HidSixAxisSensorAttribute.
    u32 reserved;                                       ///< Reserved
} HidSixAxisSensorState;

/// HidSixAxisSensorStateAtomicStorage
typedef struct HidSixAxisSensorStateAtomicStorage {
    u64 sampling_number;                                ///< SamplingNumber
    HidSixAxisSensorState state;
} HidSixAxisSensorStateAtomicStorage;

/// HidNpadSixAxisSensorLifo
typedef struct HidNpadSixAxisSensorLifo {
    HidCommonLifoHeader header;
    HidSixAxisSensorStateAtomicStorage storage[17];
} HidNpadSixAxisSensorLifo;

/// NpadSystemProperties
typedef struct {
    u64 is_charging : 3;                                             ///< Use \ref hidGetNpadPowerInfoSingle / \ref hidGetNpadPowerInfoSplit instead of accessing this directly.
    u64 is_powered : 3;                                              ///< Use \ref hidGetNpadPowerInfoSingle / \ref hidGetNpadPowerInfoSplit instead of accessing this directly.

    u64 bit6 : 1;                                                    ///< Unused
    u64 bit7 : 1;                                                    ///< Unused
    u64 bit8 : 1;                                                    ///< Unused
    u64 is_unsupported_button_pressed_on_npad_system : 1;            ///< IsUnsupportedButtonPressedOnNpadSystem
    u64 is_unsupported_button_pressed_on_npad_system_ext : 1;        ///< IsUnsupportedButtonPressedOnNpadSystemExt

    u64 is_abxy_button_oriented : 1;                                 ///< IsAbxyButtonOriented
    u64 is_sl_sr_button_oriented : 1;                                ///< IsSlSrButtonOriented
    u64 is_plus_available : 1;                                       ///< [4.0.0+] IsPlusAvailable
    u64 is_minus_available : 1;                                      ///< [4.0.0+] IsMinusAvailable
    u64 is_directional_buttons_available : 1;                        ///< [8.0.0+] IsDirectionalButtonsAvailable

    u64 unused : 48;                                                 ///< Unused
} HidNpadSystemProperties;

/// NpadSystemButtonProperties
typedef struct {
    u32 is_unintended_home_button_input_protection_enabled : 1;      ///< IsUnintendedHomeButtonInputProtectionEnabled
} HidNpadSystemButtonProperties;

/// HidPowerInfo (system)
typedef struct {
    bool is_powered;      ///< IsPowered
    bool is_charging;     ///< IsCharging
    u8 reserved[6];       ///< Reserved
    u32 battery_level;    ///< BatteryLevel, always 0-4.
} HidPowerInfo;

/// XcdDeviceHandle
typedef struct XcdDeviceHandle {
    u64 handle;
} XcdDeviceHandle;

/// HidNfcXcdDeviceHandleStateImpl
typedef struct HidNfcXcdDeviceHandleStateImpl {
    XcdDeviceHandle handle;
    u8 is_available;
    u8 is_activated;
    u8 reserved[6];
    u64 sampling_number;                                ///< SamplingNumber
} HidNfcXcdDeviceHandleStateImpl;

/// HidNfcXcdDeviceHandleStateImplAtomicStorage
typedef struct HidNfcXcdDeviceHandleStateImplAtomicStorage {
    u64 sampling_number;                                ///< SamplingNumber
    HidNfcXcdDeviceHandleStateImpl state;               ///< \ref HidNfcXcdDeviceHandleStateImpl
} HidNfcXcdDeviceHandleStateImplAtomicStorage;

/// HidNfcXcdDeviceHandleState
typedef struct HidNfcXcdDeviceHandleState {
    HidCommonLifoHeader header;
    HidNfcXcdDeviceHandleStateImplAtomicStorage storage[2];
} HidNfcXcdDeviceHandleState;

/// HidNpadInternalState
typedef struct HidNpadInternalState {
    u32 style_set;                               ///< Bitfield of \ref HidNpadStyleTag.
    u32 joy_assignment_mode;                     ///< \ref HidNpadJoyAssignmentMode
    HidNpadFullKeyColorState full_key_color;     ///< \ref HidNpadFullKeyColorState
    HidNpadJoyColorState joy_color;              ///< \ref HidNpadJoyColorState

    HidNpadCommonLifo full_key_lifo;             ///< FullKeyLifo
    HidNpadCommonLifo handheld_lifo;             ///< HandheldLifo
    HidNpadCommonLifo joy_dual_lifo;             ///< JoyDualLifo
    HidNpadCommonLifo joy_left_lifo;             ///< JoyLeftLifo
    HidNpadCommonLifo joy_right_lifo;            ///< JoyRightLifo
    HidNpadCommonLifo palma_lifo;                ///< PalmaLifo
    HidNpadCommonLifo system_ext_lifo;           ///< SystemExtLifo

    HidNpadSixAxisSensorLifo full_key_six_axis_sensor_lifo;                         ///< FullKeySixAxisSensorLifo
    HidNpadSixAxisSensorLifo handheld_six_axis_sensor_lifo;                         ///< HandheldSixAxisSensorLifo
    HidNpadSixAxisSensorLifo joy_dual_left_six_axis_sensor_lifo;                    ///< JoyDualLeftSixAxisSensorLifo
    HidNpadSixAxisSensorLifo joy_dual_right_six_axis_sensor_lifo;                   ///< JoyDualRightSixAxisSensorLifo
    HidNpadSixAxisSensorLifo joy_left_six_axis_sensor_lifo;                         ///< JoyLeftSixAxisSensorLifo
    HidNpadSixAxisSensorLifo joy_right_six_axis_sensor_lifo;                        ///< JoyRightSixAxisSensorLifo

    u32 device_type;                             ///< Bitfield of \ref HidDeviceTypeBits.
    u32 reserved;                                ///< Reserved
    HidNpadSystemProperties system_properties;
    HidNpadSystemButtonProperties system_button_properties;
    u32 battery_level[3];
    union {
        struct { // [1.0.0-3.0.2]
            HidNfcXcdDeviceHandleState nfc_xcd_device_handle;
        };

        struct {
            u32 applet_footer_ui_attribute;                                         ///< Bitfield of AppletFooterUiAttribute.
            u8 applet_footer_ui_type;                                               ///< \ref HidAppletFooterUiType
            u8 reserved_x41AD[0x5B];
        };
    };
    u8 reserved_x4208[0x20];                                                        ///< Mutex on pre-10.0.0.
    HidNpadGcTriggerLifo gc_trigger_lifo;
    u32 lark_type_l_and_main;                                                       ///< \ref HidNpadLarkType
    u32 lark_type_r;                                                                ///< \ref HidNpadLarkType
    u32 lucia_type;                                                                 ///< \ref HidNpadLuciaType
    u32 lager_type;                                                                 ///< \ref HidNpadLagerType
} HidNpadInternalState;

/// HidNpadSharedMemoryEntry
typedef struct HidNpadSharedMemoryEntry {
    HidNpadInternalState internal_state;
    u8 pad[0xC10];
} HidNpadSharedMemoryEntry;

/// HidNpadSharedMemoryFormat
typedef struct HidNpadSharedMemoryFormat {
    HidNpadSharedMemoryEntry entries[10];
} HidNpadSharedMemoryFormat;

// End HidNpad

// Begin HidGesture

/// HidGesturePoint
typedef struct HidGesturePoint {
    u32 x;                                              ///< X
    u32 y;                                              ///< Y
} HidGesturePoint;

/// HidGestureState
typedef struct HidGestureState {
    u64 sampling_number;                                ///< SamplingNumber
    u64 context_number;                                 ///< ContextNumber
    u32 type;                                           ///< \ref HidGestureType
    u32 direction;                                      ///< \ref HidGestureDirection
    u32 x;                                              ///< X
    u32 y;                                              ///< Y
    s32 delta_x;                                        ///< DeltaX
    s32 delta_y;                                        ///< DeltaY
    float velocity_x;                                   ///< VelocityX
    float velocity_y;                                   ///< VelocityY
    u32 attributes;                                     ///< Bitfield of \ref HidGestureAttribute.
    float scale;                                        ///< Scale
    float rotation_angle;                               ///< RotationAngle
    s32 point_count;                                    ///< Number of entries in the points array.
    HidGesturePoint points[4];                          ///< Array of \ref HidGesturePoint with the above count.
} HidGestureState;

/// HidGestureDummyStateAtomicStorage
typedef struct HidGestureDummyStateAtomicStorage {
    u64 sampling_number;                                ///< SamplingNumber
    HidGestureState state;
} HidGestureDummyStateAtomicStorage;

/// HidGestureLifo
typedef struct HidGestureLifo {
    HidCommonLifoHeader header;
    HidGestureDummyStateAtomicStorage storage[17];
} HidGestureLifo;

/// HidGestureSharedMemoryFormat
typedef struct HidGestureSharedMemoryFormat {
    HidGestureLifo lifo;
    u8 pad[0xF8];
} HidGestureSharedMemoryFormat;

// End HidGesture

/// HidConsoleSixAxisSensor
typedef struct {
    u64 sampling_number;                                ///< SamplingNumber
    u8 is_seven_six_axis_sensor_at_rest;                ///< IsSevenSixAxisSensorAtRest
    u8 pad[0x3];                                        ///< Padding
    float verticalization_error;                        ///< VerticalizationError
    UtilFloat3 gyro_bias;                               ///< GyroBias
    u8 pad2[0x4];                                       ///< Padding
} HidConsoleSixAxisSensor;

/// HidSharedMemory
typedef struct HidSharedMemory {
    HidDebugPadSharedMemoryFormat debug_pad;
    HidTouchScreenSharedMemoryFormat touchscreen;
    HidMouseSharedMemoryFormat mouse;
    HidKeyboardSharedMemoryFormat keyboard;
    union {
        HidBasicXpadSharedMemoryFormat basic_xpad;      ///< [1.0.0-9.2.0] BasicXpad
        HidDigitizerSharedMemoryFormat digitizer;       ///< [10.0.0+] Digitizer
    };
    HidHomeButtonSharedMemoryFormat home_button;
    HidSleepButtonSharedMemoryFormat sleep_button;
    HidCaptureButtonSharedMemoryFormat capture_button;
    HidInputDetectorSharedMemoryFormat input_detector;
    HidUniquePadSharedMemoryFormat unique_pad;          ///< [1.0.0-4.1.0] UniquePad
    HidNpadSharedMemoryFormat npad;
    HidGestureSharedMemoryFormat gesture;
    HidConsoleSixAxisSensor console_six_axis_sensor;    ///< [5.0.0+] ConsoleSixAxisSensor
    u8 unk_x3C220[0x3DE0];
} HidSharedMemory;



/* ******************************************************

 * Pretty-print helpers (Thanks ChatGPT)

 ********************************************************/

static void indentf(int level, const char* fmt, ...) {
    for (int i = 0; i < level; ++i) std::printf("  ");
    va_list ap; va_start(ap, fmt);
    std::vprintf(fmt, ap);
    va_end(ap);
}

static const char* yesno(bool b){ return b ? "yes" : "no"; }

template<typename T>
static inline T clamp_count(T val, T maxv){ return (val > maxv) ? maxv : val; }

// Print bitmask against a table of {bit, name}
static void print_bits_u32(uint32_t v, const std::vector<std::pair<uint32_t,const char*>>& tbl, int level){
    bool first=true;
    for (auto &p: tbl){
        if (v & p.first){
            if (first){ indentf(level, "bits: "); first=false; }
            std::printf("%s ", p.second);
        }
    }
    if (!first) std::printf("\n");
}

static void print_bits_u64(uint64_t v, const std::vector<std::pair<uint64_t,const char*>>& tbl, int level){
    bool first=true;
    for (auto &p: tbl){
        if (v & p.first){
            if (first){ indentf(level, "bits: "); first=false; }
            std::printf("%s ", p.second);
        }
    }
    if (!first) std::printf("\n");
}

// -----------------------------
// Enum/bit tables (partial – add more if you want nicer names)
// -----------------------------
static const std::vector<std::pair<uint32_t,const char*>> DebugPadButtonTbl = {
    {BIT(0),"A"},{BIT(1),"B"},{BIT(2),"X"},{BIT(3),"Y"},
    {BIT(4),"L"},{BIT(5),"R"},{BIT(6),"ZL"},{BIT(7),"ZR"},
    {BIT(8),"Start"},{BIT(9),"Select"},
    {BIT(10),"Left"},{BIT(11),"Up"},{BIT(12),"Right"},{BIT(13),"Down"},
};
static const std::vector<std::pair<uint32_t,const char*>> DebugPadAttrTbl = {
    {BIT(0),"IsConnected"},
};
static const std::vector<std::pair<uint32_t,const char*>> TouchAttrTbl = {
    {BIT(0),"Start"}, {BIT(1),"End"},
};
static const std::vector<std::pair<uint32_t,const char*>> MouseBtnTbl = {
    {BIT(0),"Left"},{BIT(1),"Right"},{BIT(2),"Middle"},{BIT(3),"Forward"},{BIT(4),"Back"},
};
static const std::vector<std::pair<uint32_t,const char*>> MouseAttrTbl = {
    {BIT(0),"Transferable"},{BIT(1),"IsConnected"},
};
static const std::vector<std::pair<uint32_t,const char*>> KbdModTbl = {
    {BIT(0),"Ctrl"},{BIT(1),"Shift"},{BIT(2),"LAlt"},{BIT(3),"RAlt"},{BIT(4),"GUI"},
    {BIT(8),"Caps"},{BIT(9),"Scroll"},{BIT(10),"Num"},
};
static const std::vector<std::pair<uint64_t,const char*>> NpadBtnTbl64 = {
    {BITL(0),"A"},{BITL(1),"B"},{BITL(2),"X"},{BITL(3),"Y"},
    {BITL(4),"StickL"},{BITL(5),"StickR"},
    {BITL(6),"L"},{BITL(7),"R"},{BITL(8),"ZL"},{BITL(9),"ZR"},
    {BITL(10),"Plus"},{BITL(11),"Minus"},
    {BITL(12),"Left"},{BITL(13),"Up"},{BITL(14),"Right"},{BITL(15),"Down"},
    {BITL(24),"LeftSL"},{BITL(25),"LeftSR"},{BITL(26),"RightSL"},{BITL(27),"RightSR"},
};
static const std::vector<std::pair<uint32_t,const char*>> NpadAttrTbl = {
    {BIT(0),"IsConnected"},{BIT(1),"IsWired"},{BIT(2),"IsLeftConnected"},{BIT(3),"IsLeftWired"},
    {BIT(4),"IsRightConnected"},{BIT(5),"IsRightWired"},
};
static const std::vector<std::pair<uint32_t,const char*>> NpadStyleTbl = {
    {BIT(0),"FullKey"},{BIT(1),"Handheld"},{BIT(2),"JoyDual"},{BIT(3),"JoyLeft"},{BIT(4),"JoyRight"},
    {BIT(5),"Gc"},{BIT(6),"Palma"},{BIT(7),"Lark"},{BIT(8),"HandheldLark"},
    {BIT(9),"Lucia"},{BIT(10),"Lagon"},{BIT(11),"Lager"},{BIT(29),"SystemExt"},{BIT(30),"System"},
};
static const std::vector<std::pair<uint32_t,const char*>> SixAxisAttrTbl = {
    {BIT(0),"IsConnected"},{BIT(1),"IsInterpolated"},
};

// -----------------------------
// Printers for small structs
// -----------------------------
static void print(const HidCommonLifoHeader& v, int L){
    indentf(L, "HidCommonLifoHeader:\n");
    indentf(L+1, "unused         : 0x%016" PRIx64 "\n", v.unused);
    indentf(L+1, "buffer_count   : %" PRIu64 "\n", v.buffer_count);
    indentf(L+1, "tail           : %" PRIu64 "\n", v.tail);
    indentf(L+1, "count          : %" PRIu64 "\n", v.count);
}

static void print(const HidAnalogStickState& v, int L, const char* name){
    indentf(L,"%s: x=%d y=%d\n", name, v.x, v.y);
}

static void print(const HidVector& v, int L, const char* name){
    indentf(L,"%s: x=%g y=%g z=%g\n", name, v.x, v.y, v.z);
}

static void print(const HidDirectionState& d, int L){
    indentf(L,"Direction 3x3:\n");
    for (int r=0;r<3;++r){
        indentf(L+1,"[ %g %g %g ]\n", d.direction[r][0], d.direction[r][1], d.direction[r][2]);
    }
}

// -----------------------------
// DebugPad
// -----------------------------
static void print(const HidDebugPadState& s, int L, int idx){
    indentf(L,"HidDebugPadState[%d]:\n", idx);
    indentf(L+1,"sampling_number : %" PRIu64 "\n", s.sampling_number);
    indentf(L+1,"attributes      : 0x%08" PRIx32 "\n", s.attributes);
    print_bits_u32(s.attributes, DebugPadAttrTbl, L+2);
    indentf(L+1,"buttons         : 0x%08" PRIx32 "\n", s.buttons);
    print_bits_u32(s.buttons, DebugPadButtonTbl, L+2);
    print(s.analog_stick_r, L+1, "analog_stick_r");
    print(s.analog_stick_l, L+1, "analog_stick_l");
}
static void print(const HidDebugPadLifo& lifo, int L){
    indentf(L,"HidDebugPadLifo:\n");
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        indentf(L+1,"Storage[%d] sampling=%" PRIu64 "\n", i, lifo.storage[i].sampling_number);
        print(lifo.storage[i].state, L+2, i);
    }
}

static void print(const HidDebugPadSharedMemoryFormat& v, int L){
    indentf(L,"HidDebugPadSharedMemoryFormat:\n");
    print(v.lifo, L+1);
}

// -----------------------------
// Touchscreen
// -----------------------------
static void print(const HidTouchState& t, int L, int i){
    indentf(L,"Touch[%d]: dt=%" PRIu64 " attr=0x%08" PRIx32 " fid=%u x=%u y=%u dx=%u dy=%u rot=%u\n",
            i, t.delta_time, t.attributes, t.finger_id, t.x, t.y, t.diameter_x, t.diameter_y, t.rotation_angle);
    print_bits_u32(t.attributes, TouchAttrTbl, L+1);
}

static void print(const HidTouchScreenState& s, int L, int idx){
    indentf(L,"HidTouchScreenState[%d]: samp=%" PRIu64 " count=%d\n", idx, s.sampling_number, s.count);
    int c = std::min(std::max(s.count,0), 16);
    for (int i=0;i<c;++i) print(s.touches[i], L+1, i);
}

static void print(const HidTouchScreenLifo& lifo, int L){
    indentf(L,"HidTouchScreenLifo:\n");
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        indentf(L+1,"Storage[%d] sampling=%" PRIu64 "\n", i, lifo.storage[i].sampling_number);
        print(lifo.storage[i].state, L+2, i);
    }
}

static void print(const HidTouchScreenSharedMemoryFormat& v, int L){
    indentf(L,"HidTouchScreenSharedMemoryFormat:\n");
    print(v.lifo, L+1);
}

// -----------------------------
// Mouse
// -----------------------------
static void print(const HidMouseState& s, int L, int i){
    indentf(L,"HidMouseState[%d]: samp=%" PRIu64 " x=%d y=%d dx=%d dy=%d wheel_dx=%d wheel_dy=%d\n",
            i, s.sampling_number, s.x, s.y, s.delta_x, s.delta_y, s.wheel_delta_x, s.wheel_delta_y);
    indentf(L+1,"buttons  : 0x%08" PRIx32 "\n", s.buttons);
    print_bits_u32(s.buttons, MouseBtnTbl, L+2);
    indentf(L+1,"attr     : 0x%08" PRIx32 "\n", s.attributes);
    print_bits_u32(s.attributes, MouseAttrTbl, L+2);
}
static void print(const HidMouseLifo& lifo, int L){
    indentf(L,"HidMouseLifo:\n");
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        indentf(L+1,"Storage[%d] sampling=%" PRIu64 "\n", i, lifo.storage[i].sampling_number);
        print(lifo.storage[i].state, L+2, i);
    }
}
static void print(const HidMouseSharedMemoryFormat& v, int L){
    indentf(L,"HidMouseSharedMemoryFormat:\n");
    print(v.lifo, L+1);
}

// -----------------------------
// Keyboard
// -----------------------------
static void print(const HidKeyboardState& s, int L, int idx){
    indentf(L,"HidKeyboardState[%d]: samp=%" PRIu64 "\n", idx, s.sampling_number);
    indentf(L+1,"modifiers: 0x%016" PRIx64 "\n", s.modifiers);
    print_bits_u32((uint32_t)s.modifiers, KbdModTbl, L+2);
    for (int k=0;k<4;++k)
        indentf(L+1,"keys[%d]=0x%016" PRIx64 "\n", k, s.keys[k]);
}
static void print(const HidKeyboardLifo& lifo, int L){
    indentf(L,"HidKeyboardLifo:\n");
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        indentf(L+1,"Storage[%d] sampling=%" PRIu64 "\n", i, lifo.storage[i].sampling_number);
        print(lifo.storage[i].state, L+2, i);
    }
}
static void print(const HidKeyboardSharedMemoryFormat& v, int L){
    indentf(L,"HidKeyboardSharedMemoryFormat:\n");
    print(v.lifo, L+1);
}

// -----------------------------
// BasicXpad or Digitizer (only headers + some fields to keep output manageable)
// -----------------------------
static void print(const HidBasicXpadState& s, int L, int i){
    indentf(L,"HidBasicXpadState[%d]: samp=%" PRIu64 " attr=0x%08" PRIx32 " btn=0x%08" PRIx32 " L=0x%016" PRIx64 " R=0x%016" PRIx64 "\n",
            i, s.sampling_number, s.attributes, s.buttons, s.analog_stick_left, s.analog_stick_right);
}
static void print(const HidBasicXpadLifo& lifo, int L){
    indentf(L,"HidBasicXpadLifo:\n");
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        indentf(L+1,"Storage[%d] sampling=%" PRIu64 "\n", i, lifo.storage[i].sampling_number);
        print(lifo.storage[i].state, L+2, i);
    }
}
static void print(const HidBasicXpadSharedMemoryEntry& e, int L){
    indentf(L,"HidBasicXpadSharedMemoryEntry:\n");
    print(e.lifo, L+1);
}
static void print(const HidBasicXpadSharedMemoryFormat& f, int L){
    indentf(L,"HidBasicXpadSharedMemoryFormat:\n");
    for (int i=0;i<4;++i) print(f.entries[i], L+1);
}

static void print(const HidDigitizerState& s, int L, int i){
    indentf(L,"HidDigitizerState[%d]: samp=%" PRIu64 " attr=0x%08" PRIx32 " btn=0x%08" PRIx32 "\n",
            i, s.sampling_number, s.attributes, s.buttons);
}
static void print(const HidDigitizerLifo& lifo, int L){
    indentf(L,"HidDigitizerLifo:\n");
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        indentf(L+1,"Storage[%d] sampling=%" PRIu64 "\n", i, lifo.storage[i].sampling_number);
        print(lifo.storage[i].state, L+2, i);
    }
}
static void print(const HidDigitizerSharedMemoryFormat& f, int L){
    indentf(L,"HidDigitizerSharedMemoryFormat:\n");
    print(f.lifo, L+1);
}

// -----------------------------
// Simple button sections
// -----------------------------
template<typename LifoT, typename StateT>
static void print_simple_button_lifo(const LifoT& lifo, int L, const char* name){
    indentf(L, "%s:\n", name);
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        auto &st = lifo.storage[i].state;
        indentf(L+1,"[%d] samp=%" PRIu64 " buttons=0x%016" PRIx64 "\n", i, st.sampling_number, st.buttons);
    }
}

// -----------------------------
// Input Detector
// -----------------------------
static void print(const HidInputDetectorLifo& lifo, int L){
    indentf(L,"HidInputDetectorLifo:\n");
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)2);
    for (int i=0;i<cnt;++i){
        auto &st = lifo.storage[i].state;
        indentf(L+1,"[%d] input_source_state=0x%016" PRIx64 " samp=%" PRIu64 "\n", i, st.input_source_state, st.sampling_number);
    }
}
static void print(const HidInputDetectorSharedMemoryEntry& e, int L){
    indentf(L,"HidInputDetectorSharedMemoryEntry:\n");
    print(e.lifo, L+1);
}
static void print(const HidInputDetectorSharedMemoryFormat& f, int L){
    indentf(L,"HidInputDetectorSharedMemoryFormat:\n");
    for (int i=0;i<16;++i) print(f.entries[i], L+1);
}

// -----------------------------
// UniquePad (summarized to headers to keep noise reasonable)
// -----------------------------
static void print(const HidUniquePadConfig& c, int L){
    indentf(L,"HidUniquePadConfig:\n");
    indentf(L+1,"type=%u interface=%u ctrl#=%u active=%s samp=%" PRIu64 "\n",
            c.type, c.interface, c.controller_number, yesno(c.is_active), c.sampling_number);
    indentf(L+1,"serial: ");
    for (int i=0;i<16;++i) std::printf("%02X", c.serial_number[i]);
    std::printf("\n");
}
static void print(const HidUniquePadConfigLifo& lifo, int L){
    indentf(L,"HidUniquePadConfigLifo:\n");
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)2);
    for (int i=0;i<cnt;++i){
        indentf(L+1,"Storage[%d] sampling=%" PRIu64 "\n", i, lifo.storage[i].sampling_number);
        print(lifo.storage[i].config, L+2);
    }
}
static void print(const HidUniquePadLifo& lifo, int L){
    indentf(L,"HidUniquePadLifo:\n");
    print(lifo.config_lifo, L+1);
    // Analog stick calib / sixaxis calib lifos: print just headers
    for (int a=0;a<2;++a){
        indentf(L+1,"AnalogStickCalibrationLifo[%d]:\n", a);
        print(lifo.analog_stick_calib_lifo[a].header, L+2);
    }
    indentf(L+1,"SixAxisCalibrationLifo:\n");
    print(lifo.sixaxis_calib_lifo.header, L+2);
}
static void print(const HidUniquePadSharedMemoryEntry& e, int L){
    indentf(L,"HidUniquePadSharedMemoryEntry:\n");
    print(e.lifo, L+1);
}
static void print(const HidUniquePadSharedMemoryFormat& f, int L){
    indentf(L,"HidUniquePadSharedMemoryFormat:\n");
    for (int i=0;i<16;++i) print(f.entries[i], L+1);
}

// -----------------------------
// Npad
// -----------------------------
static void print(const HidNpadControllerColor& c, int L, const char* name){
    indentf(L,"%s: main=0x%08" PRIx32 " sub=0x%08" PRIx32 "\n", name, c.main, c.sub);
}
static void print(const HidNpadFullKeyColorState& s, int L){
    indentf(L,"FullKeyColorState: attr=%u\n", s.attribute);
    print(s.full_key, L+1, "full_key");
}
static void print(const HidNpadJoyColorState& s, int L){
    indentf(L,"JoyColorState: attr=%u\n", s.attribute);
    print(s.left, L+1, "left");
    print(s.right, L+1, "right");
}
static void print(const HidNpadCommonState& s, int L, const char* title){
    indentf(L,"%s:\n", title);
    indentf(L+1,"samp=%" PRIu64 "\n", s.sampling_number);
    indentf(L+1,"buttons=0x%016" PRIx64 "\n", s.buttons);
    print_bits_u64(s.buttons, NpadBtnTbl64, L+2);
    print(s.analog_stick_l, L+1, "analog_stick_l");
    print(s.analog_stick_r, L+1, "analog_stick_r");
    indentf(L+1,"attributes=0x%08" PRIx32 "\n", s.attributes);
    print_bits_u32(s.attributes, NpadAttrTbl, L+2);
}
static void print(const HidNpadGcState& s, int L){
    indentf(L,"HidNpadGcState:\n");
    indentf(L+1,"samp=%" PRIu64 "\n", s.sampling_number);
    indentf(L+1,"buttons=0x%016" PRIx64 "\n", s.buttons);
    print_bits_u64(s.buttons, NpadBtnTbl64, L+2);
    print(s.analog_stick_l, L+1, "analog_stick_l");
    print(s.analog_stick_r, L+1, "analog_stick_r");
    indentf(L+1,"attributes=0x%08" PRIx32 "\n", s.attributes);
    print_bits_u32(s.attributes, NpadAttrTbl, L+2);
    indentf(L+1,"trigger_l=%u trigger_r=%u\n", s.trigger_l, s.trigger_r);
}

static void print(const HidSixAxisSensorState& s, int L, int i){
    indentf(L,"SixAxis[%d]: dt=%" PRIu64 " samp=%" PRIu64 " attr=0x%08" PRIx32 "\n",
            i, s.delta_time, s.sampling_number, s.attributes);
    print_bits_u32(s.attributes, SixAxisAttrTbl, L+1);
    print(s.acceleration, L+1, "accel");
    print(s.angular_velocity, L+1, "ang_vel");
    print(s.angle, L+1, "angle");
    print(s.direction, L+1);
}

static void print(const HidNpadCommonLifo& lifo, int L, const char* name){
    indentf(L,"%s:\n", name);
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        indentf(L+1,"[%d] sampling=%" PRIu64 "\n", i, lifo.storage[i].sampling_number);
        print(lifo.storage[i].state, L+2, "state");
    }
}

static void print(const HidNpadSixAxisSensorLifo& lifo, int L, const char* name){
    indentf(L,"%s:\n", name);
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        indentf(L+1,"[%d] sampling=%" PRIu64 "\n", i, lifo.storage[i].sampling_number);
        print(lifo.storage[i].state, L+2, i);
    }
}

static void print(const HidNpadGcTriggerLifo& lifo, int L){
    indentf(L,"GcTriggerLifo:\n");
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        auto &st = lifo.storage[i].state;
        indentf(L+1,"[%d] samp=%" PRIu64 " L=%u R=%u\n", i, st.sampling_number, st.trigger_l, st.trigger_r);
    }
}

static void print(const HidNpadInternalState& s, int L){
    indentf(L,"HidNpadInternalState:\n");
    indentf(L+1,"style_set=0x%08" PRIx32 "\n", s.style_set);
    print_bits_u32(s.style_set, NpadStyleTbl, L+2);

    indentf(L+1,"joy_assignment_mode=%u\n", s.joy_assignment_mode);
    print(s.full_key_color, L+1);
    print(s.joy_color, L+1);

    print(s.full_key_lifo, L+1, "full_key_lifo");
    print(s.handheld_lifo, L+1, "handheld_lifo");
    print(s.joy_dual_lifo, L+1, "joy_dual_lifo");
    print(s.joy_left_lifo, L+1, "joy_left_lifo");
    print(s.joy_right_lifo, L+1, "joy_right_lifo");
    print(s.palma_lifo, L+1, "palma_lifo");
    print(s.system_ext_lifo, L+1, "system_ext_lifo");

    print(s.full_key_six_axis_sensor_lifo, L+1, "full_key_six_axis_sensor_lifo");
    print(s.handheld_six_axis_sensor_lifo, L+1, "handheld_six_axis_sensor_lifo");
    print(s.joy_dual_left_six_axis_sensor_lifo, L+1, "joy_dual_left_six_axis_sensor_lifo");
    print(s.joy_dual_right_six_axis_sensor_lifo, L+1, "joy_dual_right_six_axis_sensor_lifo");
    print(s.joy_left_six_axis_sensor_lifo, L+1, "joy_left_six_axis_sensor_lifo");
    print(s.joy_right_six_axis_sensor_lifo, L+1, "joy_right_six_axis_sensor_lifo");

    indentf(L+1,"device_type=0x%08" PRIx32 "\n", s.device_type);
    indentf(L+1,"system_properties: is_abxy_oriented=%" PRIu64 " slsr_oriented=%" PRIu64 " plus_avail=%" PRIu64 " minus_avail=%" PRIu64 " dir_btns_avail=%" PRIu64 "\n",
            (uint64_t)s.system_properties.is_abxy_button_oriented,
            (uint64_t)s.system_properties.is_sl_sr_button_oriented,
            (uint64_t)s.system_properties.is_plus_available,
            (uint64_t)s.system_properties.is_minus_available,
            (uint64_t)s.system_properties.is_directional_buttons_available);
    indentf(L+1,"system_button_properties: unintended_home_protect=%u\n",
            s.system_button_properties.is_unintended_home_button_input_protection_enabled);
    indentf(L+1,"battery_level[0..2]=%u %u %u\n", s.battery_level[0], s.battery_level[1], s.battery_level[2]);

    print(s.gc_trigger_lifo, L+1);
    indentf(L+1,"lark_type_l_and_main=%u lark_type_r=%u lucia_type=%u lager_type=%u\n",
            s.lark_type_l_and_main, s.lark_type_r, s.lucia_type, s.lager_type);
}

static void print(const HidNpadSharedMemoryEntry& e, int L, int idx){
    indentf(L,"HidNpadSharedMemoryEntry[%d]:\n", idx);
    print(e.internal_state, L+1);
}
static void print(const HidNpadSharedMemoryFormat& f, int L){
    indentf(L,"HidNpadSharedMemoryFormat:\n");
    for (int i=0;i<10;++i) print(f.entries[i], L+1, i);
}

// -----------------------------
// Gesture
// -----------------------------
static void print(const HidGesturePoint& p, int L, int i){
    indentf(L,"Point[%d]: x=%u y=%u\n", i, p.x, p.y);
}
static void print(const HidGestureState& s, int L, int idx){
    indentf(L,"Gesture[%d]: samp=%" PRIu64 " ctx=%" PRIu64 " type=%u dir=%u x=%u y=%u dx=%d dy=%d vx=%g vy=%g attr=0x%08" PRIx32 " scale=%g rot=%g count=%d\n",
            idx, s.sampling_number, s.context_number, s.type, s.direction, s.x, s.y, s.delta_x, s.delta_y,
            s.velocity_x, s.velocity_y, s.attributes, s.scale, s.rotation_angle, s.point_count);
    int pc = std::min(std::max(s.point_count,0), 4);
    for (int i=0;i<pc;++i) print(s.points[i], L+1, i);
}
static void print(const HidGestureLifo& lifo, int L){
    indentf(L,"HidGestureLifo:\n");
    print(lifo.header, L+1);
    auto cnt = (int)clamp_count(lifo.header.count, (uint64_t)17);
    for (int i=0;i<cnt;++i){
        indentf(L+1,"[%d] sampling=%" PRIu64 "\n", i, lifo.storage[i].sampling_number);
        print(lifo.storage[i].state, L+2, i);
    }
}
static void print(const HidGestureSharedMemoryFormat& f, int L){
    indentf(L,"HidGestureSharedMemoryFormat:\n");
    print(f.lifo, L+1);
}

// -----------------------------
// Console Six Axis
// -----------------------------
static void print(const UtilFloat3& v, int L, const char* name){
    indentf(L,"%s: [%g, %g, %g]\n", name, v.value[0], v.value[1], v.value[2]);
}
static void print(const HidConsoleSixAxisSensor& c, int L){
    indentf(L,"HidConsoleSixAxisSensor:\n");
    indentf(L+1,"samp=%" PRIu64 " at_rest=%u vert_err=%g\n", c.sampling_number, c.is_seven_six_axis_sensor_at_rest, c.verticalization_error);
    print(c.gyro_bias, L+1, "gyro_bias");
}


// -----------------------------
// Top-level: HidSharedMemory
// -----------------------------
static void print(const HidSharedMemory& m, int L=0){
    indentf(L,"=== HidSharedMemory ===\n");
    /*print(m.debug_pad, L+1);
    print(m.touchscreen, L+1);
    print(m.mouse, L+1);
    print(m.keyboard, L+1);

    // Union: either BasicXpad (<=9.2.0) or Digitizer (>=10.0.0). We can't know version from file alone.
    // Print both headers safely (they overlap in union). Comment out the one you don't need.
    indentf(L+1,"[Union view] BasicXpad/Digitizer:\n");
    print(m.basic_xpad, L+2);          // if file comes from 1.0.0–9.2.0
    // print(m.digitizer, L+2);        // if file comes from 10.0.0+

    // Simple buttons
    print_simple_button_lifo(m.home_button.lifo, L+1, "HomeButtonLifo");
    print_simple_button_lifo(m.sleep_button.lifo, L+1, "SleepButtonLifo");
    print_simple_button_lifo(m.capture_button.lifo, L+1, "CaptureButtonLifo");

    print(m.input_detector, L+1);
    print(m.unique_pad, L+1);
    */
    print(m.npad, L+1);
    /*print(m.gesture, L+1);
    print(m.console_six_axis_sensor, L+1);

    // The large tail area and unknown block are left as size info (avoid dumping megabytes)
    indentf(L+1,"unk_x3C220 bytes: 0x%zx\n", sizeof m.unk_x3C220);
    */
}


/* --------------------------------------------------- 
                    Main
  --------------------------------------------------- */

int main(int argc, char** argv) {
    //argv should take -file <filename>
    std::string filename;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-file" && i + 1 < argc) {
            filename = argv[i + 1];
            break;
        }
    }

    //open file and read it as binary and load it in struct HidSSharedMemory
    HidSharedMemory hidMemory;
    std::ifstream file(filename, std::ios::binary);
    if (file) {
        file.read(reinterpret_cast<char*>(&hidMemory), sizeof(HidSharedMemory));
        file.close();
    } else {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return 1;
    }

    print(hidMemory);
    
    return 0;
}