#pragma once

#include <switch.h>
#include "InputState.h"

#include <array>
#include <mutex>

/*
    The console has exactly one keyboard view and one mouse view, so several physical devices
    have to be merged into one state. The merge lives here rather than in either publisher
    because both the hiddbg auto-pilot path and the MITM shared-memory path need the same
    answer, and two copies of it would drift.
*/
namespace syscon::hid
{
    constexpr size_t MaxSources = 4;

    class VirtualKeyboard
    {
    public:
        static VirtualKeyboard &Get();

        // A slot index, or -1 when every slot is taken.
        int Acquire();
        void Release(int source);

        void Update(int source, const controllerlib::KeyboardState &state);

        // Held keys are level state, so sources are OR'd: two keyboards pressing different
        // keys both register. False when no source is live.
        bool Compose(HidKeyboardState *out) const;

    private:
        mutable std::mutex m_mutex;
        std::array<bool, MaxSources> m_live{};
        std::array<controllerlib::KeyboardState, MaxSources> m_state{};
    };

    class VirtualMouse
    {
    public:
        static VirtualMouse &Get();

        int Acquire();
        void Release(int source);

        // Motion is a rate, not a level, so reports accumulate until someone drains them.
        void Accumulate(int source, const controllerlib::MouseState &state);

        // Sums every source's pending motion, integrates the absolute cursor position, and
        // resets the accumulators. False when no source is live.
        bool Drain(HidMouseState *out);

    private:
        mutable std::mutex m_mutex;
        std::array<bool, MaxSources> m_live{};
        std::array<controllerlib::MouseState, MaxSources> m_pending{};

        // The panel is 1280x720, which is also the space devtools screenshot/touch work in.
        s32 m_x = 640;
        s32 m_y = 360;
    };
} // namespace syscon::hid
