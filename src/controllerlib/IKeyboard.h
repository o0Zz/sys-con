#pragma once

#include "InputDeviceBase.h"

namespace controllerlib
{
    class IKeyboard : public InputDeviceBase
    {
    public:
        using InputDeviceBase::InputDeviceBase;

        InputDeviceKind GetKind() const override { return InputDeviceKind::Keyboard; }

        /*
            One report per call, never coalesced. A press and the release that follows it are
            two distinct states, and dropping either types a character the user did not press
            or holds one they let go of. The caller loops fast enough to drain the queue.
        */
        virtual Status ReadInput(KeyboardState *state, uint32_t timeout_us) = 0;
    };
} // namespace controllerlib
