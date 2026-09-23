#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "drivers/HIDMouseController.h"
#include "mocks/HIDDevice.h"
#include "mocks/Logger.h"

using namespace controllerlib;

namespace
{
    constexpr uint8_t BootSubClass = 0x01;
    constexpr uint8_t MouseProtocol = 0x02;

    ControllerConfig WithSensitivity(uint16_t percent)
    {
        ControllerConfig config;
        config.mouseSensitivityPercent = percent;
        return config;
    }
} // namespace

TEST(HIDMouse, test_boot_mouse_opens)
{
    HIDTestRig rig(BootMouseDescriptor, sizeof(BootMouseDescriptor), BootSubClass, MouseProtocol);
    HIDMouseController mouse(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());

    EXPECT_EQ(mouse.Initialize(), Status::Success);
}

TEST(HIDMouse, test_a_keyboard_descriptor_is_rejected)
{
    HIDTestRig rig(BootKeyboardDescriptor, sizeof(BootKeyboardDescriptor), BootSubClass, MouseProtocol);
    HIDMouseController mouse(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());

    EXPECT_EQ(mouse.Initialize(), Status::HidIsNotMouse);
}

TEST(HIDMouse, test_deltas_and_buttons_are_reported)
{
    HIDTestRig rig(BootMouseDescriptor, sizeof(BootMouseDescriptor), BootSubClass, MouseProtocol);
    rig.QueueReport({MOUSE_BUTTON_LEFT, 10, (uint8_t)-5, 1});

    HIDMouseController mouse(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());
    ASSERT_EQ(mouse.Initialize(), Status::Success);

    MouseState state{};
    ASSERT_EQ(mouse.ReadInput(&state, 0), Status::Success);

    EXPECT_EQ(state.deltaX, 10);
    EXPECT_EQ(state.deltaY, -5);
    EXPECT_EQ(state.deltaWheel, 1);
    EXPECT_EQ(state.buttons, MOUSE_BUTTON_LEFT);
}

TEST(HIDMouse, test_queued_reports_are_summed_not_replaced)
{
    HIDTestRig rig(BootMouseDescriptor, sizeof(BootMouseDescriptor), BootSubClass, MouseProtocol);
    // A mouse reports a delta, not a position: keeping only the freshest report would throw
    // away every count but the last one's.
    rig.QueueReport({0x00, 10, 10, 1});
    rig.QueueReport({0x00, 10, 10, 1});
    rig.QueueReport({0x00, 5, (uint8_t)-20, 0});

    HIDMouseController mouse(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());
    ASSERT_EQ(mouse.Initialize(), Status::Success);

    MouseState state{};
    ASSERT_EQ(mouse.ReadInput(&state, 0), Status::Success);

    EXPECT_EQ(state.deltaX, 25);
    EXPECT_EQ(state.deltaY, 0);
    EXPECT_EQ(state.deltaWheel, 2);
}

TEST(HIDMouse, test_accumulator_is_reset_between_reads)
{
    HIDTestRig rig(BootMouseDescriptor, sizeof(BootMouseDescriptor), BootSubClass, MouseProtocol);
    rig.QueueReport({0x00, 10, 0, 0});

    HIDMouseController mouse(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());
    ASSERT_EQ(mouse.Initialize(), Status::Success);

    MouseState state{};
    ASSERT_EQ(mouse.ReadInput(&state, 0), Status::Success);
    EXPECT_EQ(state.deltaX, 10);

    EXPECT_EQ(mouse.ReadInput(&state, 0), Status::Timeout);
}

TEST(HIDMouse, test_sensitivity_scales_the_deltas)
{
    HIDTestRig rig(BootMouseDescriptor, sizeof(BootMouseDescriptor), BootSubClass, MouseProtocol);
    rig.QueueReport({0x00, 100, (uint8_t)-50, 3});

    HIDMouseController mouse(rig.TakeDevice(), WithSensitivity(50), std::make_unique<MockLogger>());
    ASSERT_EQ(mouse.Initialize(), Status::Success);

    MouseState state{};
    ASSERT_EQ(mouse.ReadInput(&state, 0), Status::Success);

    EXPECT_EQ(state.deltaX, 50);
    EXPECT_EQ(state.deltaY, -25);
    // The wheel is a notch count, not a distance, so sensitivity must not touch it.
    EXPECT_EQ(state.deltaWheel, 3);
}

TEST(HIDMouse, test_side_buttons_map_to_their_usage_numbers)
{
    HIDTestRig rig(BootMouseDescriptor, sizeof(BootMouseDescriptor), BootSubClass, MouseProtocol);
    rig.QueueReport({MOUSE_BUTTON_BACK | MOUSE_BUTTON_FORWARD, 0, 0, 0});

    HIDMouseController mouse(rig.TakeDevice(), ControllerConfig{}, std::make_unique<MockLogger>());
    ASSERT_EQ(mouse.Initialize(), Status::Success);

    MouseState state{};
    ASSERT_EQ(mouse.ReadInput(&state, 0), Status::Success);

    EXPECT_EQ(state.buttons, MOUSE_BUTTON_BACK | MOUSE_BUTTON_FORWARD);
}
