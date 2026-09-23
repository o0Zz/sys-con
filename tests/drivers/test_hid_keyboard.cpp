#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "drivers/HIDKeyboardController.h"
#include "mocks/HIDDevice.h"
#include "mocks/Logger.h"

using namespace controllerlib;

namespace
{
    constexpr uint8_t KeyA = 0x04;
    constexpr uint8_t KeyB = 0x05;
    constexpr uint8_t KeyCapsLock = 0x39;
    constexpr uint8_t ErrorRollOver = 0x01;

    constexpr uint8_t BootSubClass = 0x01;
    constexpr uint8_t KeyboardProtocol = 0x01;
} // namespace

TEST(HIDKeyboard, test_boot_keyboard_opens)
{
    HIDTestRig rig(BootKeyboardDescriptor, sizeof(BootKeyboardDescriptor), BootSubClass, KeyboardProtocol);
    HIDKeyboardController keyboard(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());

    EXPECT_EQ(keyboard.Initialize(), Status::Success);
}

TEST(HIDKeyboard, test_a_mouse_descriptor_is_rejected)
{
    HIDTestRig rig(BootMouseDescriptor, sizeof(BootMouseDescriptor), BootSubClass, KeyboardProtocol);
    HIDKeyboardController keyboard(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());

    EXPECT_EQ(keyboard.Initialize(), Status::HidIsNotKeyboard);
}

TEST(HIDKeyboard, test_set_protocol_failure_fails_initialize)
{
    HIDTestRig rig(BootKeyboardDescriptor, sizeof(BootKeyboardDescriptor), BootSubClass, KeyboardProtocol);

    using ::testing::_;
    using ::testing::Return;
    // 0x0B is SET_PROTOCOL; SET_IDLE and the LED report must keep succeeding.
    EXPECT_CALL(*rig.Interface(), ControlTransferOutput(_, 0x0B, _, _, _, _)).WillRepeatedly(Return(Status::WriteFailed));

    HIDKeyboardController keyboard(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());
    EXPECT_EQ(keyboard.Initialize(), Status::HidProtocolFailed);
}

TEST(HIDKeyboard, test_set_protocol_is_not_sent_on_a_non_boot_interface)
{
    HIDTestRig rig(BootKeyboardDescriptor, sizeof(BootKeyboardDescriptor), 0x00, 0x00);

    using ::testing::_;
    using ::testing::Return;
    EXPECT_CALL(*rig.Interface(), ControlTransferOutput(_, _, _, _, _, _)).WillRepeatedly(Return(Status::Success));
    EXPECT_CALL(*rig.Interface(), ControlTransferOutput(_, 0x0B, _, _, _, _)).Times(0);

    HIDKeyboardController keyboard(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());
    EXPECT_EQ(keyboard.Initialize(), Status::Success);
}

TEST(HIDKeyboard, test_modifiers_and_keys_are_reported)
{
    HIDTestRig rig(BootKeyboardDescriptor, sizeof(BootKeyboardDescriptor), BootSubClass, KeyboardProtocol);
    rig.QueueReport({KEYBOARD_MOD_LEFT_SHIFT, 0x00, KeyA, KeyB, 0, 0, 0, 0});

    HIDKeyboardController keyboard(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());
    ASSERT_EQ(keyboard.Initialize(), Status::Success);

    KeyboardState state{};
    ASSERT_EQ(keyboard.ReadInput(&state, 0), Status::Success);

    EXPECT_EQ(state.modifiers, KEYBOARD_MOD_LEFT_SHIFT);
    EXPECT_EQ(state.keyCount, 2);
    EXPECT_EQ(state.keys[0], KeyA);
    EXPECT_EQ(state.keys[1], KeyB);
}

TEST(HIDKeyboard, test_release_is_not_swallowed_by_the_press_before_it)
{
    HIDTestRig rig(BootKeyboardDescriptor, sizeof(BootKeyboardDescriptor), BootSubClass, KeyboardProtocol);
    // Both land inside one poll window. A keep-latest read would publish only the release,
    // and the keystroke would never reach the console.
    rig.QueueReport({0x00, 0x00, KeyA, 0, 0, 0, 0, 0});
    rig.QueueReport({0x00, 0x00, 0, 0, 0, 0, 0, 0});

    HIDKeyboardController keyboard(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());
    ASSERT_EQ(keyboard.Initialize(), Status::Success);

    KeyboardState pressed{};
    ASSERT_EQ(keyboard.ReadInput(&pressed, 0), Status::Success);
    EXPECT_EQ(pressed.keyCount, 1);
    EXPECT_EQ(pressed.keys[0], KeyA);

    KeyboardState released{};
    ASSERT_EQ(keyboard.ReadInput(&released, 0), Status::Success);
    EXPECT_EQ(released.keyCount, 0);
}

TEST(HIDKeyboard, test_rollover_does_not_release_held_keys)
{
    HIDTestRig rig(BootKeyboardDescriptor, sizeof(BootKeyboardDescriptor), BootSubClass, KeyboardProtocol);
    rig.QueueReport({0x00, 0x00, ErrorRollOver, ErrorRollOver, ErrorRollOver, 0, 0, 0});

    HIDKeyboardController keyboard(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());
    ASSERT_EQ(keyboard.Initialize(), Status::Success);

    KeyboardState state{};
    EXPECT_EQ(keyboard.ReadInput(&state, 0), Status::NothingTodo);
}

TEST(HIDKeyboard, test_caps_lock_toggles_on_press_and_not_while_held)
{
    HIDTestRig rig(BootKeyboardDescriptor, sizeof(BootKeyboardDescriptor), BootSubClass, KeyboardProtocol);
    rig.QueueReport({0x00, 0x00, KeyCapsLock, 0, 0, 0, 0, 0}); // pressed  -> on
    rig.QueueReport({0x00, 0x00, KeyCapsLock, 0, 0, 0, 0, 0}); // held     -> still on
    rig.QueueReport({0x00, 0x00, 0, 0, 0, 0, 0, 0});           // released -> still on
    rig.QueueReport({0x00, 0x00, KeyCapsLock, 0, 0, 0, 0, 0}); // pressed  -> off

    HIDKeyboardController keyboard(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());
    ASSERT_EQ(keyboard.Initialize(), Status::Success);

    KeyboardState state{};
    ASSERT_EQ(keyboard.ReadInput(&state, 0), Status::Success);
    EXPECT_EQ(state.locks & KEYBOARD_LOCK_CAPS, KEYBOARD_LOCK_CAPS);

    ASSERT_EQ(keyboard.ReadInput(&state, 0), Status::Success);
    EXPECT_EQ(state.locks & KEYBOARD_LOCK_CAPS, KEYBOARD_LOCK_CAPS);

    ASSERT_EQ(keyboard.ReadInput(&state, 0), Status::Success);
    EXPECT_EQ(state.locks & KEYBOARD_LOCK_CAPS, KEYBOARD_LOCK_CAPS);

    ASSERT_EQ(keyboard.ReadInput(&state, 0), Status::Success);
    EXPECT_EQ(state.locks & KEYBOARD_LOCK_CAPS, 0);
}
