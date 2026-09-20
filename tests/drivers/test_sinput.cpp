#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "drivers/SInputController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"
#include "mocks/USBEndpoint.h"
#include "mocks/USBInterface.h"
#include <cstdint>
#include <cstring>

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
using namespace controllerlib;

namespace
{
    MATCHER_P2(BufferMatches, expected, size, "Matches buffer content")
    {
        return std::memcmp(arg, expected, size) == 0;
    }

    SInputController MakeController()
    {
        ControllerConfig config;
        return SInputController(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    }

    // A report with both sticks centred and both triggers released, as the device sends it.
    class Report
    {
    public:
        Report()
        {
            memset(m_bytes, 0, sizeof(m_bytes));
            Data()->report_id = SINPUT_REPORT_ID_INPUT;
            Data()->trigger_l = INT16_MIN;
            Data()->trigger_r = INT16_MIN;
        }

        SInputButtonData *Data() { return reinterpret_cast<SInputButtonData *>(m_bytes); }
        uint8_t *Bytes() { return m_bytes; }
        static constexpr size_t Size = SINPUT_INPUT_BUFFER_SIZE;

    private:
        uint8_t m_bytes[SINPUT_INPUT_BUFFER_SIZE];
    };
} // namespace

TEST(Controller, test_sinput_face_buttons)
{
    RawInputData rawData;
    uint16_t input_idx = 0;
    SInputController controller = MakeController();

    Report report;
    report.Data()->south = 1;
    report.Data()->north = 1;

    EXPECT_EQ(controller.ParseData(report.Bytes(), Report::Size, &rawData, &input_idx), Status::Success);

    EXPECT_TRUE(rawData.buttons[1]);
    EXPECT_FALSE(rawData.buttons[2]);
    EXPECT_FALSE(rawData.buttons[3]);
    EXPECT_TRUE(rawData.buttons[4]);
}

TEST(Controller, test_sinput_last_button_bit_does_not_reach_the_dpad_pins)
{
    RawInputData rawData;
    uint16_t input_idx = 0;
    SInputController controller = MakeController();

    Report report;
    report.Data()->misc_10 = 1;

    EXPECT_EQ(controller.ParseData(report.Bytes(), Report::Size, &rawData, &input_idx), Status::Success);

    EXPECT_TRUE(rawData.buttons[28]);
    EXPECT_FALSE(rawData.buttons[DPAD_UP_BUTTON_ID]);
    EXPECT_FALSE(rawData.buttons[DPAD_DOWN_BUTTON_ID]);
    EXPECT_FALSE(rawData.buttons[DPAD_LEFT_BUTTON_ID]);
    EXPECT_FALSE(rawData.buttons[DPAD_RIGHT_BUTTON_ID]);
}

TEST(Controller, test_sinput_dpad)
{
    RawInputData rawData;
    uint16_t input_idx = 0;
    SInputController controller = MakeController();

    Report report;
    report.Data()->dpad_up = 1;
    report.Data()->dpad_left = 1;

    EXPECT_EQ(controller.ParseData(report.Bytes(), Report::Size, &rawData, &input_idx), Status::Success);

    EXPECT_TRUE(rawData.buttons[DPAD_UP_BUTTON_ID]);
    EXPECT_TRUE(rawData.buttons[DPAD_LEFT_BUTTON_ID]);
    EXPECT_FALSE(rawData.buttons[DPAD_DOWN_BUTTON_ID]);
    EXPECT_FALSE(rawData.buttons[DPAD_RIGHT_BUTTON_ID]);
}

TEST(Controller, test_sinput_sticks)
{
    RawInputData rawData;
    uint16_t input_idx = 0;
    SInputController controller = MakeController();

    Report report;
    report.Data()->left_x = INT16_MIN;
    report.Data()->left_y = INT16_MAX;
    report.Data()->right_x = INT16_MAX;
    report.Data()->right_y = INT16_MIN;

    EXPECT_EQ(controller.ParseData(report.Bytes(), Report::Size, &rawData, &input_idx), Status::Success);

    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::X], -1.0f);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Y], 1.0f);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Z], 1.0f);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Rz], -1.0f);
}

TEST(Controller, test_sinput_triggers)
{
    RawInputData rawData;
    uint16_t input_idx = 0;
    SInputController controller = MakeController();

    Report report;
    EXPECT_EQ(controller.ParseData(report.Bytes(), Report::Size, &rawData, &input_idx), Status::Success);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Rx], -1.0f);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Ry], -1.0f);

    report.Data()->trigger_r = INT16_MAX;
    EXPECT_EQ(controller.ParseData(report.Bytes(), Report::Size, &rawData, &input_idx), Status::Success);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Rx], -1.0f);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Ry], 1.0f);
}

TEST(Controller, test_sinput_command_reply_is_ignored)
{
    RawInputData rawData;
    uint16_t input_idx = 0;
    SInputController controller = MakeController();

    Report report;
    report.Data()->report_id = SINPUT_REPORT_ID_REPLY;

    EXPECT_EQ(controller.ParseData(report.Bytes(), Report::Size, &rawData, &input_idx), Status::NothingTodo);
}

TEST(Controller, test_sinput_rejects_truncated_report)
{
    RawInputData rawData;
    uint16_t input_idx = 0;
    SInputController controller = MakeController();

    Report report;

    for (size_t size = 0; size < sizeof(SInputButtonData); size++)
        EXPECT_EQ(controller.ParseData(report.Bytes(), size, &rawData, &input_idx), Status::UnexpectedData) << "size=" << size;
}

TEST(Controller, test_sinput_rumble)
{
    ControllerConfig config;

    auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
    auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
    EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(Status::Success));
    EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(Status::Success));

    uint8_t expected[SINPUT_COMMAND_BUFFER_SIZE]{
        SINPUT_REPORT_ID_COMMAND, SINPUT_COMMAND_HAPTIC, SINPUT_HAPTIC_TYPE_RUMBLE, 0x7f, 0x00, 0xff, 0x00};

    EXPECT_CALL(*mockUSBEndpointOut, Write(BufferMatches(expected, sizeof(expected)), sizeof(expected)))
        .Times(1)
        .WillOnce(testing::Return(Status::Success));

    SInputController controller(std::make_unique<MockDevice>(0x2e8a, 0x10c6, std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut))), config, std::make_unique<MockLogger>());

    EXPECT_EQ(controller.Initialize(), Status::Success);
    EXPECT_TRUE(controller.Support(SUPPORTS_RUMBLE));
    EXPECT_EQ(controller.SetRumble(0, 1.0f, 0.5f), Status::Success);
}

TEST(Controller, test_sinput_rumble_without_output_endpoint)
{
    SInputController controller = MakeController();

    EXPECT_EQ(controller.SetRumble(0, 1.0f, 1.0f), Status::InvalidIndex);
}
