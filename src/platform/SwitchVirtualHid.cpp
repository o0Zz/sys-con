#include "SwitchVirtualHid.h"

#include <algorithm>

using namespace controllerlib;

namespace syscon::hid
{
    namespace
    {
        constexpr s32 ScreenWidth = 1280;
        constexpr s32 ScreenHeight = 720;

        // HidKeyboardModifier has no left/right split for Control, Shift or Gui, and the lock
        // keys live there too although USB reports them as ordinary presses.
        constexpr struct
        {
            uint8_t usb;
            u64 npad;
        } Modifiers[] = {
            {KEYBOARD_MOD_LEFT_CTRL, HidKeyboardModifier_Control},
            {KEYBOARD_MOD_RIGHT_CTRL, HidKeyboardModifier_Control},
            {KEYBOARD_MOD_LEFT_SHIFT, HidKeyboardModifier_Shift},
            {KEYBOARD_MOD_RIGHT_SHIFT, HidKeyboardModifier_Shift},
            {KEYBOARD_MOD_LEFT_ALT, HidKeyboardModifier_LeftAlt},
            {KEYBOARD_MOD_RIGHT_ALT, HidKeyboardModifier_RightAlt},
            {KEYBOARD_MOD_LEFT_GUI, HidKeyboardModifier_Gui},
            {KEYBOARD_MOD_RIGHT_GUI, HidKeyboardModifier_Gui},
        };

        constexpr struct
        {
            uint8_t lock;
            u64 npad;
        } Locks[] = {
            {KEYBOARD_LOCK_CAPS, HidKeyboardModifier_CapsLock},
            {KEYBOARD_LOCK_SCROLL, HidKeyboardModifier_ScrollLock},
            {KEYBOARD_LOCK_NUM, HidKeyboardModifier_NumLock},
        };

        // USB button usage order is left/right/middle/back/forward; libnx has back and
        // forward the other way round.
        constexpr struct
        {
            uint8_t usb;
            u32 npad;
        } MouseButtons[] = {
            {MOUSE_BUTTON_LEFT, HidMouseButton_Left},
            {MOUSE_BUTTON_RIGHT, HidMouseButton_Right},
            {MOUSE_BUTTON_MIDDLE, HidMouseButton_Middle},
            {MOUSE_BUTTON_BACK, HidMouseButton_Back},
            {MOUSE_BUTTON_FORWARD, HidMouseButton_Forward},
        };

        int AcquireSlot(std::array<bool, MaxSources> &live)
        {
            for (size_t i = 0; i < live.size(); i++)
            {
                if (live[i])
                    continue;

                live[i] = true;
                return (int)i;
            }

            return -1;
        }
    } // namespace

    VirtualKeyboard &VirtualKeyboard::Get()
    {
        static VirtualKeyboard instance;
        return instance;
    }

    int VirtualKeyboard::Acquire()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return AcquireSlot(m_live);
    }

    void VirtualKeyboard::Release(int source)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_live[source] = false;
        m_state[source] = KeyboardState{};
    }

    void VirtualKeyboard::Update(int source, const KeyboardState &state)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state[source] = state;
    }

    bool VirtualKeyboard::Compose(HidKeyboardState *out) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        bool any = false;
        *out = HidKeyboardState{};

        for (size_t i = 0; i < MaxSources; i++)
        {
            if (!m_live[i])
                continue;

            any = true;
            const KeyboardState &state = m_state[i];

            for (const auto &entry : Modifiers)
            {
                if (state.modifiers & entry.usb)
                    out->modifiers |= entry.npad;
            }

            for (const auto &entry : Locks)
            {
                if (state.locks & entry.lock)
                    out->modifiers |= entry.npad;
            }

            for (uint8_t k = 0; k < state.keyCount; k++)
            {
                const uint8_t key = state.keys[k];
                if (key == 0)
                    continue;

                out->keys[key / 64] |= 1ull << (key % 64);
            }

            // hid reports a held modifier both in the modifier field and as its usage id.
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                if (state.modifiers & (1u << bit))
                {
                    const uint8_t usage = (uint8_t)(HidKeyboardKey_LeftControl + bit);
                    out->keys[usage / 64] |= 1ull << (usage % 64);
                }
            }
        }

        return any;
    }

    VirtualMouse &VirtualMouse::Get()
    {
        static VirtualMouse instance;
        return instance;
    }

    int VirtualMouse::Acquire()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return AcquireSlot(m_live);
    }

    void VirtualMouse::Release(int source)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_live[source] = false;
        m_pending[source] = MouseState{};
    }

    void VirtualMouse::Accumulate(int source, const MouseState &state)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_pending[source].deltaX += state.deltaX;
        m_pending[source].deltaY += state.deltaY;
        m_pending[source].deltaWheel += state.deltaWheel;
        m_pending[source].buttons = state.buttons;
    }

    bool VirtualMouse::Drain(HidMouseState *out)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        bool any = false;
        *out = HidMouseState{};

        for (size_t i = 0; i < MaxSources; i++)
        {
            if (!m_live[i])
                continue;

            any = true;
            MouseState &pending = m_pending[i];

            out->delta_x += pending.deltaX;
            out->delta_y += pending.deltaY;
            out->wheel_delta_y += pending.deltaWheel;

            for (const auto &entry : MouseButtons)
            {
                if (pending.buttons & entry.usb)
                    out->buttons |= entry.npad;
            }

            pending.deltaX = 0;
            pending.deltaY = 0;
            pending.deltaWheel = 0;
        }

        if (!any)
            return false;

        m_x = std::clamp(m_x + out->delta_x, 0, ScreenWidth - 1);
        m_y = std::clamp(m_y + out->delta_y, 0, ScreenHeight - 1);

        out->x = m_x;
        out->y = m_y;
        out->attributes = HidMouseAttribute_IsConnected | HidMouseAttribute_Transferable;

        return true;
    }
} // namespace syscon::hid
