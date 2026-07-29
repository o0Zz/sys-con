#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Controllers/SteamController2026.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"
#include "mocks/USBInterface.h"
#include "mocks/USBEndpoint.h"

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

    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), CONTROLLER_STATUS_SUCCESS);

    EXPECT_TRUE(controller.IsControllerConnected(input_idx));
    EXPECT_FALSE(rawData.buttons[1]);
    EXPECT_FLOAT_EQ(rawData.analog[ControllerAnalogType_X], BaseController::Normalize(0x0241, -32768, 32767));
    EXPECT_FLOAT_EQ(rawData.analog[ControllerAnalogType_Y], BaseController::Normalize(-static_cast<int16_t>(0xFED2), -32768, 32767));
}

TEST(Controller, test_steam2026_misc_report_ignored)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    SteamController2026 controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[9] = {0x41, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), CONTROLLER_STATUS_NOTHING_TODO);
    EXPECT_FALSE(controller.IsControllerConnected(input_idx));
}

TEST(Controller, test_steam2026_wireless_disconnected)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    SteamController2026 controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[2] = {0x46, 0x01};
    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), CONTROLLER_STATUS_NOTHING_TODO);
    EXPECT_FALSE(controller.IsControllerConnected(input_idx));
}
