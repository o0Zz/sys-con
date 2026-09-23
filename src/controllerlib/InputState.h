#pragma once

#include <array>
#include <cstdint>

namespace controllerlib
{
    // Which of the three input interfaces a device implements. The host dispatches on this
    // instead of a downcast: the device build is compiled -fno-rtti.
    enum class InputDeviceKind : uint8_t
    {
        Gamepad,
        Keyboard,
        Mouse,
    };

    inline constexpr size_t KeyboardMaxKeys = 6;

    // USB HID 1.11 section 8.3 bit order. A host's own modifier layout will differ; the
    // translation belongs there, not here.
    enum KeyboardModifier : uint8_t
    {
        KEYBOARD_MOD_LEFT_CTRL = 0x01,
        KEYBOARD_MOD_LEFT_SHIFT = 0x02,
        KEYBOARD_MOD_LEFT_ALT = 0x04,
        KEYBOARD_MOD_LEFT_GUI = 0x08,
        KEYBOARD_MOD_RIGHT_CTRL = 0x10,
        KEYBOARD_MOD_RIGHT_SHIFT = 0x20,
        KEYBOARD_MOD_RIGHT_ALT = 0x40,
        KEYBOARD_MOD_RIGHT_GUI = 0x80,
    };

    // A USB keyboard reports the lock keys as ordinary presses and leaves the state to the
    // host, so the driver holds it and drives the LEDs.
    enum KeyboardLock : uint8_t
    {
        KEYBOARD_LOCK_NUM = 0x01,
        KEYBOARD_LOCK_CAPS = 0x02,
        KEYBOARD_LOCK_SCROLL = 0x04,
    };

    // The full set of keys held right now, not an edge. keys[] holds usage page 0x07 scan
    // codes; 0 is an empty slot.
    struct KeyboardState
    {
        uint8_t modifiers{0};
        uint8_t locks{0};
        uint8_t keyCount{0};
        std::array<uint8_t, KeyboardMaxKeys> keys{};
    };

    // USB HID button usage order. Note that a host may order back/forward differently.
    enum MouseButton : uint8_t
    {
        MOUSE_BUTTON_LEFT = 0x01,
        MOUSE_BUTTON_RIGHT = 0x02,
        MOUSE_BUTTON_MIDDLE = 0x04,
        MOUSE_BUTTON_BACK = 0x08,
        MOUSE_BUTTON_FORWARD = 0x10,
    };

    // Deltas are summed over every report drained by one read, so no motion is lost when the
    // device reports faster than the host polls. They are relative to the previous read, and
    // the reader must not accumulate them again.
    struct MouseState
    {
        int32_t deltaX{0};
        int32_t deltaY{0};
        int32_t deltaWheel{0};
        uint8_t buttons{0};
    };
} // namespace controllerlib
