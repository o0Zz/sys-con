#pragma once

#include "Status.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

// HIDKeyboard and HIDMouse belong to HIDDataInterpreter and live in the global namespace; see
// GenericHIDController.h for why the forward declarations stay out here.
class HIDKeyboard;
class HIDMouse;

namespace controllerlib
{
    // Interface subclass and protocol codes of a keyboard or mouse (HID 1.11, 4.2 and 4.3).
    inline constexpr uint8_t HidSubclassBoot = 0x01;
    inline constexpr uint8_t HidProtocolKeyboard = 0x01;
    inline constexpr uint8_t HidProtocolMouse = 0x02;

    inline constexpr uint8_t HidRequestTypeStandardInterfaceIn = 0x81;
    inline constexpr uint8_t HidRequestGetDescriptor = 0x06;
    inline constexpr uint16_t HidReportDescriptorValue = 0x22 << 8;

    inline constexpr uint8_t HidRequestTypeClassInterfaceOut = 0x21;
    inline constexpr uint8_t HidRequestSetProtocol = 0x0B;
    // A boot-capable device may have been left in the boot protocol by a previous host; the
    // report descriptor only describes the report protocol.
    inline constexpr uint16_t HidReportProtocol = 1;

    inline constexpr uint8_t HidUsageLeftControl = 0xE0;

    struct KeyboardState
    {
        // Bit n is HID usage n, the eight modifier usages 0xE0-0xE7 included.
        std::array<uint64_t, 4> keys{};
        // Bit 0 LeftControl, 1 LeftShift, 2 LeftAlt, 3 LeftGui, 4-7 the right ones.
        uint8_t modifiers{0};

        bool IsPressed(uint8_t usage) const { return (keys[usage / 64] >> (usage % 64)) & 1; }
    };

    struct MouseReport
    {
        // Bit n is HID button n + 1: left, right, middle, back, forward, ...
        uint8_t buttons{0};
        int16_t delta_x{0};
        int16_t delta_y{0};
        int16_t wheel{0};
    };

    class KeyboardDecoder
    {
        std::shared_ptr<HIDKeyboard> m_keyboard;

    public:
        KeyboardDecoder();
        ~KeyboardDecoder();

        // HidIsNotKeyboard: the report descriptor describes no keyboard.
        Status Initialize(const uint8_t *descriptor, size_t size);

        // NothingTodo: not a keyboard report (another report id), or the keyboard reported a
        // rollover or self-test error instead of its keys; the previous state still stands.
        Status Parse(const uint8_t *report, size_t size, KeyboardState *state) const;
    };

    class MouseDecoder
    {
        std::shared_ptr<HIDMouse> m_mouse;

    public:
        MouseDecoder();
        ~MouseDecoder();

        // HidIsNotMouse: the report descriptor describes no mouse.
        Status Initialize(const uint8_t *descriptor, size_t size);

        // NothingTodo: not a mouse report (another report id).
        Status Parse(const uint8_t *report, size_t size, MouseReport *mouse) const;
    };
} // namespace controllerlib
