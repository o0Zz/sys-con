#include "HidKeyboardMouse.h"
#include "HIDReportDescriptor.h"
#include "HIDKeyboard.h"
#include "HIDMouse.h"

namespace controllerlib
{
    namespace
    {
        constexpr uint8_t HidUsageFirstKey = 0x04;

        void Press(KeyboardState *state, uint8_t usage)
        {
            state->keys[usage / 64] |= uint64_t{1} << (usage % 64);
        }

        std::shared_ptr<HIDReportDescriptor> ParseDescriptor(const uint8_t *descriptor, size_t size)
        {
            return std::make_shared<HIDReportDescriptor>(descriptor, static_cast<uint16_t>(size));
        }
    } // namespace

    KeyboardDecoder::KeyboardDecoder() = default;
    KeyboardDecoder::~KeyboardDecoder() = default;

    Status KeyboardDecoder::Initialize(const uint8_t *descriptor, size_t size)
    {
        m_keyboard = std::make_shared<HIDKeyboard>(ParseDescriptor(descriptor, size));
        return m_keyboard->is_valid() ? Status::Success : Status::HidIsNotKeyboard;
    }

    Status KeyboardDecoder::Parse(const uint8_t *report, size_t size, KeyboardState *state) const
    {
        HIDKeyboardData data;
        if (!m_keyboard->parse_data(const_cast<uint8_t *>(report), static_cast<uint16_t>(size), &data))
            return Status::NothingTodo;

        KeyboardState parsed;
        parsed.modifiers = data.modifiers;

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if ((parsed.modifiers >> bit) & 1)
                Press(&parsed, HidUsageLeftControl + bit);
        }

        for (uint8_t i = 0; i < data.key_count; i++)
        {
            const uint8_t usage = static_cast<uint8_t>(data.keys[i]);

            // ErrorRollOver, POSTFail, ErrorUndefined
            if (usage < HidUsageFirstKey)
                return Status::NothingTodo;

            Press(&parsed, usage);
        }

        *state = parsed;
        return Status::Success;
    }

    MouseDecoder::MouseDecoder() = default;
    MouseDecoder::~MouseDecoder() = default;

    Status MouseDecoder::Initialize(const uint8_t *descriptor, size_t size)
    {
        m_mouse = std::make_shared<HIDMouse>(ParseDescriptor(descriptor, size));
        return m_mouse->is_valid() ? Status::Success : Status::HidIsNotMouse;
    }

    Status MouseDecoder::Parse(const uint8_t *report, size_t size, MouseReport *mouse) const
    {
        HIDMouseData data;
        if (!m_mouse->parse_data(const_cast<uint8_t *>(report), static_cast<uint16_t>(size), &data))
            return Status::NothingTodo;

        MouseReport parsed;
        // HID buttons are numbered from 1.
        for (uint8_t button = 1; button < data.button_count; button++)
        {
            if (data.buttons[button])
                parsed.buttons |= 1 << (button - 1);
        }
        parsed.delta_x = data.x;
        parsed.delta_y = data.y;
        parsed.wheel = data.wheel;

        *mouse = parsed;
        return Status::Success;
    }
} // namespace controllerlib
