#pragma once

#include "InputDeviceBase.h"

namespace controllerlib
{
    class IMouse : public InputDeviceBase
    {
    public:
        using InputDeviceBase::InputDeviceBase;

        InputDeviceKind GetKind() const override { return InputDeviceKind::Mouse; }

        // Deltas of every report queued on the endpoint, summed; buttons from the last one.
        virtual Status ReadInput(MouseState *state, uint32_t timeout_us) = 0;
    };
} // namespace controllerlib
