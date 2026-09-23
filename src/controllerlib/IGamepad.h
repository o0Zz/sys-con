#pragma once

#include "InputDeviceBase.h"

namespace controllerlib
{
    struct NormalizedStick
    {
        float axis_x{0.0f};
        float axis_y{0.0f};
    };

    struct NormalizedButtonData
    {
        // Indexed by GamepadButton, not by pin (RawInputData::buttons is the pin-indexed one).
        GamepadButtonStates buttons{};
        NormalizedStick sticks[2]{};
    };

    class IGamepad : public InputDeviceBase
    {
    public:
        using InputDeviceBase::InputDeviceBase;

        InputDeviceKind GetKind() const override { return InputDeviceKind::Gamepad; }

        virtual uint16_t GetInputCount() = 0;
        virtual Status ReadInput(NormalizedButtonData *normalData, uint16_t *input_idx, uint32_t timeout_us) = 0;

        virtual bool Support(ControllerFeature feature) const = 0;

        virtual Status SetRumble(uint16_t input_idx, float amp_high, float amp_low) = 0;

        virtual bool IsControllerConnected(uint16_t input_idx)
        {
            (void)input_idx;
            return true;
        }
    };
} // namespace controllerlib
