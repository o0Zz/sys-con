#pragma once
#include "AnalogAxis.h"
#include "GamepadButton.h"

#include <array>
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
    AnalogAxis bind{AnalogAxis::Unknown};
};

struct ControllerComboConfig
{
    GamepadButton buttonSimulated{GamepadButton::NONE};
    GamepadButton buttons[2];
};

class ControllerConfig
{
public:
    std::string driver;
    std::string profile;

    uint32_t inputMaxPacketSize{0};
    uint32_t outputMaxPacketSize{0};

    ControllerType controllerType{ControllerType_Pro};
    // EnumArray, so these can only be indexed with an AnalogAxis. The previous plain arrays
    // were a trap: `uint8_t analogFactorPercent[N]{100}` sets only element 0 to 100 and the
    // rest to 0, which is why the constructor below had to re-fill them by hand.
    AnalogPercentages analogDeadzonePercent{0};
    AnalogPercentages analogFactorPercent{100};

    // Each button can be driven by up to MAX_PIN_BY_BUTTONS pins; 0 means unmapped.
    EnumArray<GamepadButton, std::array<uint8_t, MAX_PIN_BY_BUTTONS>, GamepadButtonCount> buttonsPin{};

    bool buttonsAnalogUsed{false};
    EnumArray<GamepadButton, ControllerAnalogConfig, GamepadButtonCount> buttonsAnalog{};

    ControllerComboConfig simulateCombos[MAX_CONTROLLER_COMBO];

    RGBAColor bodyColor{0, 0, 0, 255};
    RGBAColor buttonsColor{0, 0, 0, 255};
    RGBAColor leftGripColor{0, 0, 0, 255};
    RGBAColor rightGripColor{0, 0, 0, 255};

    ControllerConfig()
    {
        for (int i = 0; i < MAX_CONTROLLER_COMBO; i++)
        {
            simulateCombos[i].buttonSimulated = GamepadButton::NONE;
            for (int j = 0; j < 2; j++)
                simulateCombos[i].buttons[j] = GamepadButton::NONE;
        }

    }
};