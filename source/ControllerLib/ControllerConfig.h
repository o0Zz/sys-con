#pragma once
#include <cstdint>
#include <string.h>
#include <string>

#define MAX_JOYSTICKS      2
#define MAX_PIN_BY_BUTTONS 2

#define MAX_HID_CONTROLLER_BUTTONS 32

#define DPAD_UP_BUTTON_ID    MAX_HID_CONTROLLER_BUTTONS + 0
#define DPAD_DOWN_BUTTON_ID  MAX_HID_CONTROLLER_BUTTONS + 1
#define DPAD_LEFT_BUTTON_ID  MAX_HID_CONTROLLER_BUTTONS + 2
#define DPAD_RIGHT_BUTTON_ID MAX_HID_CONTROLLER_BUTTONS + 3

#define MAX_CONTROLLER_BUTTONS 36
#define MAX_CONTROLLER_COMBO   16

enum ControllerButton
{
    NONE = 0,
    X,
    A,
    B,
    Y,
    LSTICK_CLICK,
    LSTICK_LEFT,
    LSTICK_RIGHT,
    LSTICK_UP,
    LSTICK_DOWN,
    RSTICK_CLICK,
    RSTICK_LEFT,
    RSTICK_RIGHT,
    RSTICK_UP,
    RSTICK_DOWN,
    L,
    R,
    ZL,
    ZR,
    MINUS,
    PLUS,
    DPAD_UP,
    DPAD_RIGHT,
    DPAD_DOWN,
    DPAD_LEFT,
    CAPTURE,
    HOME,

    COUNT
};

union RGBAColor
{
    struct
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };
    uint8_t values[4];
    uint32_t rgbaValue;
};

enum ControllerAnalogBinding
{
    ControllerAnalogBinding_Unknown = 0,
    ControllerAnalogBinding_X,
    ControllerAnalogBinding_Y,
    ControllerAnalogBinding_Z,
    ControllerAnalogBinding_Rz,
    ControllerAnalogBinding_Rx,
    ControllerAnalogBinding_Ry,
    ControllerAnalogBinding_Slider,
    ControllerAnalogBinding_Dial,
    ControllerAnalogBinding_Brake,
    ControllerAnalogBinding_Accelerator,

    ControllerAnalogBinding_Count
};

enum ControllerType
{
    ControllerType_Unknown = 0,
    ControllerType_Pro,
    ControllerType_ProWithBattery,
    ControllerType_Tarragon,
    ControllerType_Snes,
    ControllerType_PokeballPlus,
    ControllerType_Gamecube,
    ControllerType_3rdPartyPro,
    ControllerType_N64,
    ControllerType_Sega,
    ControllerType_Nes,
    ControllerType_Famicom
};

struct ControllerAnalogConfig
{
    float sign{1.0};
    ControllerAnalogBinding bind{ControllerAnalogBinding::ControllerAnalogBinding_Unknown};
};

struct ControllerComboConfig
{
    ControllerButton buttonSimulated{ControllerButton::NONE};
    ControllerButton buttons[2];
};

class ControllerConfig
{
public:
    std::string driver;
    std::string profile;

    uint32_t inputMaxPacketSize{0};
    uint32_t outputMaxPacketSize{0};

    ControllerType controllerType{ControllerType_Pro};
    uint8_t analogDeadzonePercent[ControllerAnalogBinding_Count]{0};
    uint8_t analogFactorPercent[ControllerAnalogBinding_Count]{100};

    uint8_t buttonsPin[ControllerButton::COUNT][MAX_PIN_BY_BUTTONS]{0};

    bool buttonsAnalogUsed{false};
    ControllerAnalogConfig buttonsAnalog[ControllerButton::COUNT]{0};

    ControllerComboConfig simulateCombos[MAX_CONTROLLER_COMBO];

    RGBAColor bodyColor{0, 0, 0, 255};
    RGBAColor buttonsColor{0, 0, 0, 255};
    RGBAColor leftGripColor{0, 0, 0, 255};
    RGBAColor rightGripColor{0, 0, 0, 255};

    ControllerConfig()
    {
        for (int i = 0; i < ControllerAnalogBinding_Count; i++)
        {
            analogDeadzonePercent[i] = 0;
            analogFactorPercent[i] = 100;
        }

        for (int i = 0; i < MAX_CONTROLLER_COMBO; i++)
        {
            simulateCombos[i].buttonSimulated = ControllerButton::NONE;
            for (int j = 0; j < 2; j++)
                simulateCombos[i].buttons[j] = ControllerButton::NONE;
        }

        memset(buttonsPin, 0, sizeof(buttonsPin));
    }
};