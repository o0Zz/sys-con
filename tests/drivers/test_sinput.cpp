#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "drivers/SInputController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"
#include "mocks/USBEndpoint.h"
#include "mocks/USBInterface.h"
#include <cstdint>
#include <cstring>

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

namespace
{
    void FeedFeaturesReply(SInputController &controller, uint8_t feature_flags, uint16_t accel_range_g, uint16_t gyro_range_dps)
    {
        uint8_t reply[SINPUT_INPUT_BUFFER_SIZE]{SINPUT_REPORT_ID_REPLY, SINPUT_COMMAND_FEATURES};
        SInputFeatures features{};
        features.feature_flags_0 = feature_flags;
        features.accel_range_g = accel_range_g;
        features.gyro_range_dps = gyro_range_dps;
        memcpy(reply + 2, &features, sizeof(features));

        RawInputData rawData;
        uint16_t input_idx = 0;
        EXPECT_EQ(controller.ParseData(reply, sizeof(reply), &rawData, &input_idx), Status::NothingTodo);
    }
} // namespace

TEST(Controller, test_sinput_motion_is_off_until_the_features_reply)
{
    RawInputData rawData;
    uint16_t input_idx = 0;
    SInputController controller = MakeController();

    Report report;
    report.Data()->accel_z = 4096;

    EXPECT_EQ(controller.ParseData(report.Bytes(), Report::Size, &rawData, &input_idx), Status::Success);

    EXPECT_FALSE(controller.Support(SUPPORTS_MOTION));
    EXPECT_FLOAT_EQ(rawData.motion.accel[1], 0.0f);
}

TEST(Controller, test_sinput_motion_scaled_by_reported_ranges)
{
    RawInputData rawData;
    uint16_t input_idx = 0;
    SInputController controller = MakeController();

    FeedFeaturesReply(controller, SINPUT_FEATURE_ACCELEROMETER | SINPUT_FEATURE_GYROSCOPE, 8, 2000);
    EXPECT_TRUE(controller.Support(SUPPORTS_MOTION));

    Report report;
    report.Data()->accel_z = 4096; // 1 g at +/-8 g full scale
    report.Data()->gyro_x = 16384; // 1000 dps at +/-2000 dps full scale
    report.Data()->gyro_y = -16384;

    EXPECT_EQ(controller.ParseData(report.Bytes(), Report::Size, &rawData, &input_idx), Status::Success);

    EXPECT_FLOAT_EQ(rawData.motion.accel[0], 0.0f);
    EXPECT_FLOAT_EQ(rawData.motion.accel[1], StandardGravity);
    EXPECT_FLOAT_EQ(rawData.motion.accel[2], 0.0f);
    EXPECT_FLOAT_EQ(rawData.motion.gyro[0], -1000.0f * RadiansPerDegree);
    EXPECT_FLOAT_EQ(rawData.motion.gyro[2], 1000.0f * RadiansPerDegree);
}

TEST(Controller, test_sinput_features_without_imu_keep_motion_off)
{
    SInputController controller = MakeController();

    FeedFeaturesReply(controller, 0x01, 8, 2000);

    EXPECT_FALSE(controller.Support(SUPPORTS_MOTION));
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

    uint8_t featuresCommand[SINPUT_COMMAND_BUFFER_SIZE]{SINPUT_REPORT_ID_COMMAND, SINPUT_COMMAND_FEATURES};
    EXPECT_CALL(*mockUSBEndpointOut, Write(BufferMatches(featuresCommand, sizeof(featuresCommand)), sizeof(featuresCommand)))
        .Times(1)
        .WillOnce(testing::Return(Status::Success));

    // An input report still queued ahead of the reply, as a streaming pad has.
    EXPECT_CALL(*mockUSBEndpointIn, Read(testing::_, testing::_, testing::_))
        .WillOnce(testing::Invoke([](uint8_t *outBuffer, size_t *size, uint64_t) {
            Report report;
            memcpy(outBuffer, report.Bytes(), Report::Size);
            *size = Report::Size;
            return Status::Success;
        }))
        .WillOnce(testing::Invoke([](uint8_t *outBuffer, size_t *size, uint64_t) {
            uint8_t reply[SINPUT_INPUT_BUFFER_SIZE]{SINPUT_REPORT_ID_REPLY, SINPUT_COMMAND_FEATURES};
            SInputFeatures features{};
            features.feature_flags_0 = SINPUT_FEATURE_GYROSCOPE;
            features.accel_range_g = 8;
            features.gyro_range_dps = 2000;
            memcpy(reply + 2, &features, sizeof(features));
            memcpy(outBuffer, reply, sizeof(reply));
            *size = sizeof(reply);
            return Status::Success;
        }));

    uint8_t expected[SINPUT_COMMAND_BUFFER_SIZE]{
        SINPUT_REPORT_ID_COMMAND, SINPUT_COMMAND_HAPTIC, SINPUT_HAPTIC_TYPE_RUMBLE, 0x7f, 0x00, 0xff, 0x00};

    EXPECT_CALL(*mockUSBEndpointOut, Write(BufferMatches(expected, sizeof(expected)), sizeof(expected)))
        .Times(1)
        .WillOnce(testing::Return(Status::Success));

    SInputController controller(std::make_unique<MockDevice>(0x2e8a, 0x10c6, std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut))), config, std::make_unique<MockLogger>());

    EXPECT_EQ(controller.Initialize(), Status::Success);
    EXPECT_TRUE(controller.Support(SUPPORTS_RUMBLE));
    EXPECT_TRUE(controller.Support(SUPPORTS_MOTION));
    EXPECT_EQ(controller.SetRumble(0, RumbleValue{.left = {.amp_low = 0.5f, .amp_high = 1.0f}}), Status::Success);
}

TEST(Controller, test_sinput_rumble_without_output_endpoint)
{
    SInputController controller = MakeController();

    EXPECT_EQ(controller.SetRumble(0, RumbleValue{.left = {.amp_low = 1.0f, .amp_high = 1.0f}}), Status::InvalidIndex);
}
