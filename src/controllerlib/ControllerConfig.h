#pragma once
#include "AnalogAxis.h"
#include "GamepadButton.h"
#include "PinId.h"

#include <array>
#include <cstdint>
#include <string.h>
#include <string>

#define MAX_PIN_BY_BUTTONS 2

#define MAX_CONTROLLER_COMBO 16

#define MAX_RUMBLE_PACKET_SIZE 64

namespace controllerlib
{
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

    // One motor's slot inside ControllerRumbleConfig::packet. size 0 means the report has no
    // field for that motor.
    struct ControllerRumbleField
    {
        uint8_t offset{0};
        uint8_t size{0};
        bool littleEndian{false};
    };

    struct ControllerRumbleConfig
    {
        std::array<uint8_t, MAX_RUMBLE_PACKET_SIZE> packet{};
        uint8_t packetSize{0};
        ControllerRumbleField low;
        ControllerRumbleField high;

        bool IsValid() const { return packetSize > 0; }
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
        // EnumArray, so these can only be indexed with an AnalogAxis, and the braced value
        // initializes every element (a plain array would set only element 0).
        AnalogPercentages analogDeadzonePercent{0};
        AnalogPercentages analogFactorPercent{100};

        // Each button can be driven by up to MAX_PIN_BY_BUTTONS pins; 0 means unmapped.
        EnumArray<GamepadButton, std::array<PinId, MAX_PIN_BY_BUTTONS>, GamepadButtonCount> buttonsPin{};

        bool buttonsAnalogUsed{false};
        EnumArray<GamepadButton, ControllerAnalogConfig, GamepadButtonCount> buttonsAnalog{};

        ControllerComboConfig simulateCombos[MAX_CONTROLLER_COMBO];

        // Only used by drivers that have no rumble of their own, GenericHIDController above all.
        ControllerRumbleConfig rumble;

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
} // namespace controllerlib
