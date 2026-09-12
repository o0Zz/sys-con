#include <gtest/gtest.h>
#include "drivers/SwitchController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"

TEST(Controller, test_switch_button1)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;
    SwitchController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[64] = {0x30, 0x0D, 0x91, 0x01, 0x80, 0x00, 0xB9, 0x77, 0x7D, 0xDB, 0xF7, 0x7B, 0x09, 0x00, 0x00, 0x00};
    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), CONTROLLER_STATUS_SUCCESS);

    EXPECT_TRUE(rawData.buttons[1]);
}

TEST(Controller, test_switch_lstick_left)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    SwitchController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[64] = {0x30, 0xFC, 0x91, 0x00, 0x80, 0x00, 0xC6, 0xB1, 0x72, 0xE5, 0xE7, 0x79, 0x03, 0x00, 0x00, 0x00};
    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), CONTROLLER_STATUS_SUCCESS);

    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::X], -1.0f);
}
