#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstring>

#include "drivers/SteamController2026.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"
#include "mocks/USBInterface.h"
#include "mocks/USBEndpoint.h"

using namespace controllerlib;

TEST(Controller, test_steam2026_input_report)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    SteamController2026 controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[54] = {
        0x42, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x02, 0x41, 0x02, 0xD2, 0xFE,
        0x8E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xD0, 0x6A, 0x0D, 0x00, 0x10, 0x14, 0xE7, 0x1B, 0xFE, 0x35, 0x27, 0x01, 0x6C, 0x00,
        0xEB, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::Success);

    EXPECT_TRUE(controller.IsControllerConnected(input_idx));
    EXPECT_FALSE(rawData.buttons[1]);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::X], BaseController::Normalize(0x0241, -32768, 32767));
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Y], BaseController::Normalize(-static_cast<int16_t>(0xFED2), -32768, 32767));
}

TEST(Controller, test_steam2026_imu_in_sdl_frame)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    SteamController2026 controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[sizeof(Steam2026InputReport)]{};
    Steam2026InputReport *report = reinterpret_cast<Steam2026InputReport *>(buffer);
    report->report_id = REPORT_INPUT;
    report->imu.sAccelZ = 16384; // 1 g at +/-2 g full scale
    report->imu.sAccelY = 16384;
    report->imu.sGyroX = 16384; // 1000 dps at +/-2000 dps full scale

    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::Success);

    EXPECT_TRUE(controller.Support(SUPPORTS_MOTION));
    EXPECT_FLOAT_EQ(rawData.motion.accel[1], StandardGravity);
    EXPECT_FLOAT_EQ(rawData.motion.accel[2], -StandardGravity);
    EXPECT_FLOAT_EQ(rawData.motion.gyro[0], 1000.0f * RadiansPerDegree);
    EXPECT_FLOAT_EQ(rawData.motion.gyro[1], 0.0f);
}

TEST(Controller, test_steam2026_misc_report_ignored)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    SteamController2026 controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[9] = {0x41, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::NothingTodo);
    EXPECT_FALSE(controller.IsControllerConnected(input_idx));
}

TEST(Controller, test_steam2026_wireless_disconnected)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    SteamController2026 controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[2] = {0x46, 0x01};
    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::NothingTodo);
    EXPECT_FALSE(controller.IsControllerConnected(input_idx));
}

MATCHER_P2(BufferMatches, expected, size, "Matches buffer content")
{
    return std::memcmp(arg, expected, size) == 0;
}

TEST(Controller, test_steam2026_rumble_is_a_triton_haptic_report)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
    auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
    MockUSBEndpoint *outEndpoint = mockUSBEndpointOut.get();
    EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(Status::Success));
    EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(Status::Success));

    auto mockUSBInterface = std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut));
    MockUSBInterface *interface = mockUSBInterface.get();
    IUSBInterface::InterfaceDescriptor descriptor{};
    EXPECT_CALL(*interface, Open).WillOnce(testing::Return(Status::Success));
    EXPECT_CALL(*interface, GetDescriptor).WillRepeatedly(testing::Return(&descriptor));
    EXPECT_CALL(*interface, ControlTransferOutput(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::Return(Status::Success));

    SteamController2026 controller(std::make_unique<MockDevice>(0x28de, 0x1304, std::move(mockUSBInterface)), config, std::make_unique<MockLogger>());
    EXPECT_EQ(controller.Initialize(), Status::Success);
    EXPECT_TRUE(controller.Support(SUPPORTS_RUMBLE));

    // Nothing is sent while the pad is not connected to the dongle.
    EXPECT_EQ(controller.SetRumble(0, 1.0f, 1.0f), Status::NothingTodo);

    uint8_t connected[2] = {REPORT_WIRELESS_STATUS, 0x02};
    EXPECT_EQ(controller.ParseData(connected, sizeof(connected), &rawData, &input_idx), Status::NothingTodo);

    /*
        Output report 0x80: type, intensity, then speed and gain per side, all little endian.
        The low frequency band drives the left side. Ref: SDL_hidapi_steam_triton.c.
    */
    uint8_t expected[]{0x80, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x7f, 0x00};

    EXPECT_CALL(*outEndpoint, Write(BufferMatches(expected, sizeof(expected)), sizeof(expected)))
        .Times(1)
        .WillOnce(testing::Return(Status::Success));

    EXPECT_EQ(controller.SetRumble(0, 0.5f, 1.0f), Status::Success);
}

TEST(Controller, test_steam2026_rumble_is_resent_before_the_pad_times_out)
{
    ControllerConfig config;
    RawInputData rawData;
    NormalizedButtonData buttonData;
    uint16_t input_idx = 0;

    IUSBEndpoint::EndpointDescriptor endpointDescriptor{};
    endpointDescriptor.wMaxPacketSize = 64;

    auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
    auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
    MockUSBEndpoint *outEndpoint = mockUSBEndpointOut.get();
    EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(Status::Success));
    EXPECT_CALL(*mockUSBEndpointIn, GetDescriptor).WillRepeatedly(testing::Return(&endpointDescriptor));
    EXPECT_CALL(*mockUSBEndpointIn, Read(testing::_, testing::_, testing::_)).WillRepeatedly(testing::Return(Status::Timeout));
    EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(Status::Success));

    auto mockUSBInterface = std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut));
    MockUSBInterface *interface = mockUSBInterface.get();
    IUSBInterface::InterfaceDescriptor descriptor{};
    EXPECT_CALL(*interface, Open).WillOnce(testing::Return(Status::Success));
    EXPECT_CALL(*interface, GetDescriptor).WillRepeatedly(testing::Return(&descriptor));
    EXPECT_CALL(*interface, ControlTransferOutput(testing::_, testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::Return(Status::Success));

    SteamController2026 controller(std::make_unique<MockDevice>(0x28de, 0x1304, std::move(mockUSBInterface)), config, std::make_unique<MockLogger>());
    EXPECT_EQ(controller.Initialize(), Status::Success);

    uint8_t connected[2] = {REPORT_WIRELESS_STATUS, 0x02};
    EXPECT_EQ(controller.ParseData(connected, sizeof(connected), &rawData, &input_idx), Status::NothingTodo);

    // One write for the SetRumble itself, then one more once the resend interval has passed.
    EXPECT_CALL(*outEndpoint, Write(testing::_, HID_RUMBLE_OUTPUT_REPORT_BYTES))
        .Times(2)
        .WillRepeatedly(testing::Return(Status::Success));

    EXPECT_EQ(controller.SetRumble(0, 1.0f, 1.0f), Status::Success);

    // Polling again straight away must not re-send: the pad is still inside its timeout.
    (void)controller.ReadInput(&buttonData, &input_idx, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(STEAMCONTROLLER_RUMBLE_RESEND_MS + 10));
    (void)controller.ReadInput(&buttonData, &input_idx, 0);
}

TEST(Controller, test_steam2026_idle_rumble_is_not_resent)
{
    ControllerConfig config;
    NormalizedButtonData buttonData;
    uint16_t input_idx = 0;

    IUSBEndpoint::EndpointDescriptor endpointDescriptor{};
    endpointDescriptor.wMaxPacketSize = 64;

    auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
    auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
    MockUSBEndpoint *outEndpoint = mockUSBEndpointOut.get();
    EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(Status::Success));
    EXPECT_CALL(*mockUSBEndpointIn, GetDescriptor).WillRepeatedly(testing::Return(&endpointDescriptor));
    EXPECT_CALL(*mockUSBEndpointIn, Read(testing::_, testing::_, testing::_)).WillRepeatedly(testing::Return(Status::Timeout));
    EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(Status::Success));

    auto mockUSBInterface = std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut));
    MockUSBInterface *interface = mockUSBInterface.get();
    EXPECT_CALL(*interface, Open).WillOnce(testing::Return(Status::Success));

    SteamController2026 controller(std::make_unique<MockDevice>(0x28de, 0x1304, std::move(mockUSBInterface)), config, std::make_unique<MockLogger>());
    EXPECT_EQ(controller.Initialize(), Status::Success);

    EXPECT_CALL(*outEndpoint, Write(testing::_, testing::_)).Times(0);

    std::this_thread::sleep_for(std::chrono::milliseconds(STEAMCONTROLLER_RUMBLE_RESEND_MS + 10));
    (void)controller.ReadInput(&buttonData, &input_idx, 0);
}
