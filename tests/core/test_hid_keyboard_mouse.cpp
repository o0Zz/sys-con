#include <gtest/gtest.h>

#include "HidKeyboardMouse.h"

using namespace controllerlib;

namespace
{
    // HID 1.11, appendix E.6: the keyboard every boot-capable keyboard also describes.
    const uint8_t KeyboardDescriptor[] = {
        0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,                         // Generic Desktop, Keyboard, Application
        0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, // Modifiers
        0x75, 0x01, 0x95, 0x08, 0x81, 0x02,                         //
        0x95, 0x01, 0x75, 0x08, 0x81, 0x01,                         // Reserved byte
        0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, // LEDs
        0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,             //
        0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,             // 6 key slots
        0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,             //
        0xC0,
    };

    // HID 1.11, appendix E.10: three buttons and 8-bit X/Y, no wheel.
    const uint8_t BootMouseDescriptor[] = {
        0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, // Mouse, Pointer
        0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, // Buttons 1-3
        0x95, 0x03, 0x75, 0x01, 0x81, 0x02,                         //
        0x95, 0x01, 0x75, 0x05, 0x81, 0x01,                         // Padding
        0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, // X, Y
        0x75, 0x08, 0x95, 0x02, 0x81, 0x06,                         //
        0xC0, 0xC0,
    };

    // Report 2: 16 buttons, 12-bit X/Y and a wheel. Report 3: consumer control (media keys).
    const uint8_t ReportIdMouseDescriptor[] = {
        0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02,             // Mouse, report id 2
        0x09, 0x01, 0xA1, 0x00,                                     // Pointer
        0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01, // Buttons 1-16
        0x95, 0x10, 0x75, 0x01, 0x81, 0x02,                         //
        0x05, 0x01, 0x16, 0x01, 0xF8, 0x26, 0xFF, 0x07,             // X, Y: -2047..2047
        0x75, 0x0C, 0x95, 0x02, 0x09, 0x30, 0x09, 0x31, 0x81, 0x06, //
        0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01,             // Wheel
        0x09, 0x38, 0x81, 0x06,                                     //
        0xC0, 0xC0,                                                 //
        0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x03,             // Consumer control, report id 3
        0x19, 0x00, 0x2A, 0xFF, 0x03, 0x15, 0x00, 0x26, 0xFF, 0x03, //
        0x75, 0x10, 0x95, 0x01, 0x81, 0x00,                         //
        0xC0,
    };

    KeyboardDecoder MakeKeyboard()
    {
        KeyboardDecoder keyboard;
        EXPECT_EQ(keyboard.Initialize(KeyboardDescriptor, sizeof(KeyboardDescriptor)), Status::Success);
        return keyboard;
    }

    MouseDecoder MakeMouse(const uint8_t *descriptor, size_t size)
    {
        MouseDecoder mouse;
        EXPECT_EQ(mouse.Initialize(descriptor, size), Status::Success);
        return mouse;
    }
} // namespace

TEST(HidKeyboardMouse, test_keyboard_report_sets_key_and_modifier_usages)
{
    KeyboardDecoder keyboard = MakeKeyboard();
    // LeftShift + RightAlt, then 'a' (0x04) and Enter (0x28)
    const uint8_t report[] = {0x42, 0x00, 0x04, 0x28, 0x00, 0x00, 0x00, 0x00};
    KeyboardState state;

    ASSERT_EQ(keyboard.Parse(report, sizeof(report), &state), Status::Success);

    EXPECT_EQ(state.modifiers, 0x42);
    EXPECT_TRUE(state.IsPressed(0x04));
    EXPECT_TRUE(state.IsPressed(0x28));
    EXPECT_TRUE(state.IsPressed(0xE1));
    EXPECT_TRUE(state.IsPressed(0xE6));
    EXPECT_FALSE(state.IsPressed(0xE0));
    EXPECT_FALSE(state.IsPressed(0x05));
    EXPECT_FALSE(state.IsPressed(0x00));
}

TEST(HidKeyboardMouse, test_keyboard_release_clears_every_key)
{
    KeyboardDecoder keyboard = MakeKeyboard();
    const uint8_t pressed[] = {0x01, 0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    const uint8_t released[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    KeyboardState state;

    ASSERT_EQ(keyboard.Parse(pressed, sizeof(pressed), &state), Status::Success);
    ASSERT_EQ(keyboard.Parse(released, sizeof(released), &state), Status::Success);

    EXPECT_EQ(state.modifiers, 0);
    for (uint64_t word : state.keys)
        EXPECT_EQ(word, 0u);
}

TEST(HidKeyboardMouse, test_keyboard_rollover_keeps_previous_state)
{
    KeyboardDecoder keyboard = MakeKeyboard();
    const uint8_t pressed[] = {0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t rollover[] = {0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
    KeyboardState state;

    ASSERT_EQ(keyboard.Parse(pressed, sizeof(pressed), &state), Status::Success);
    EXPECT_EQ(keyboard.Parse(rollover, sizeof(rollover), &state), Status::NothingTodo);

    EXPECT_TRUE(state.IsPressed(0x04));
    EXPECT_FALSE(state.IsPressed(0x01));
}

TEST(HidKeyboardMouse, test_keyboard_usages_above_63_land_in_later_words)
{
    KeyboardDecoder keyboard = MakeKeyboard();
    // F12 (0x45), KeypadEnter (0x58), Application (0x65)
    const uint8_t report[] = {0x00, 0x00, 0x45, 0x58, 0x65, 0x00, 0x00, 0x00};
    KeyboardState state;

    ASSERT_EQ(keyboard.Parse(report, sizeof(report), &state), Status::Success);

    EXPECT_EQ(state.keys[0], 0u);
    EXPECT_EQ(state.keys[1], (uint64_t{1} << (0x45 - 64)) | (uint64_t{1} << (0x58 - 64)) | (uint64_t{1} << (0x65 - 64)));
    EXPECT_EQ(state.keys[2], 0u);
    EXPECT_EQ(state.keys[3], 0u);
}

TEST(HidKeyboardMouse, test_keyboard_short_report_is_ignored)
{
    KeyboardDecoder keyboard = MakeKeyboard();
    const uint8_t report[] = {0x00, 0x00, 0x04};
    KeyboardState state;

    EXPECT_EQ(keyboard.Parse(report, sizeof(report), &state), Status::NothingTodo);
    EXPECT_FALSE(state.IsPressed(0x04));
}

TEST(HidKeyboardMouse, test_mouse_descriptor_is_not_a_keyboard)
{
    KeyboardDecoder keyboard;

    EXPECT_EQ(keyboard.Initialize(BootMouseDescriptor, sizeof(BootMouseDescriptor)), Status::HidIsNotKeyboard);
}

TEST(HidKeyboardMouse, test_boot_mouse_decodes_signed_deltas_without_wheel)
{
    MouseDecoder mouse = MakeMouse(BootMouseDescriptor, sizeof(BootMouseDescriptor));
    const uint8_t report[] = {0x05, 0xFE, 0x10};
    MouseReport parsed;

    ASSERT_EQ(mouse.Parse(report, sizeof(report), &parsed), Status::Success);

    EXPECT_EQ(parsed.buttons, 0x05);
    EXPECT_EQ(parsed.delta_x, -2);
    EXPECT_EQ(parsed.delta_y, 16);
    EXPECT_EQ(parsed.wheel, 0);
}

TEST(HidKeyboardMouse, test_report_id_mouse_decodes_12_bit_axes_and_wheel)
{
    MouseDecoder mouse = MakeMouse(ReportIdMouseDescriptor, sizeof(ReportIdMouseDescriptor));
    // Report 2, button 1 and button 5, X = -300 (0xED4), Y = 200 (0x0C8), wheel -1
    const uint8_t report[] = {0x02, 0x11, 0x00, 0xD4, 0x8E, 0x0C, 0xFF};
    MouseReport parsed;

    ASSERT_EQ(mouse.Parse(report, sizeof(report), &parsed), Status::Success);

    EXPECT_EQ(parsed.buttons, 0x11);
    EXPECT_EQ(parsed.delta_x, -300);
    EXPECT_EQ(parsed.delta_y, 200);
    EXPECT_EQ(parsed.wheel, -1);
}

TEST(HidKeyboardMouse, test_other_report_id_is_ignored)
{
    MouseDecoder mouse = MakeMouse(ReportIdMouseDescriptor, sizeof(ReportIdMouseDescriptor));
    const uint8_t consumer[] = {0x03, 0xE9, 0x00};
    MouseReport parsed;
    parsed.delta_x = 7;

    EXPECT_EQ(mouse.Parse(consumer, sizeof(consumer), &parsed), Status::NothingTodo);
    EXPECT_EQ(parsed.delta_x, 7);
}

TEST(HidKeyboardMouse, test_keyboard_descriptor_is_not_a_mouse)
{
    MouseDecoder mouse;

    EXPECT_EQ(mouse.Initialize(KeyboardDescriptor, sizeof(KeyboardDescriptor)), Status::HidIsNotMouse);
}

