#include <gtest/gtest.h>
#include "Controllers/WiiController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"

TEST(Controller, test_wii_controller_button_5)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    WiiController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[64] = {0x21,
                          0x10, 0x10, 0x00, 0x82, 0x82, 0x7B, 0x81, 0x17, 0x1A,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_EQ(controller.ParseData(buffer + 1, 9, &rawData, &input_idx), CONTROLLER_STATUS_SUCCESS);

    EXPECT_TRUE(rawData.buttons[5]);
}

TEST(Controller, test_wii_controller_disconnected)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    WiiController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[64] = {0x21,
                          0x10, 0x10, 0x00, 0x82, 0x82, 0x7B, 0x81, 0x17, 0x1A,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_EQ(controller.ParseData(buffer + 1 + 9, 9, &rawData, &input_idx), CONTROLLER_STATUS_NOTHING_TODO);
}
