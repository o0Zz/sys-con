#include <gtest/gtest.h>
#include "drivers/WiiController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
using namespace controllerlib;


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

    EXPECT_EQ(controller.ParseData(buffer + 1, 9, &rawData, &input_idx), Status::Success);

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

    EXPECT_EQ(controller.ParseData(buffer + 1 + 9, 9, &rawData, &input_idx), Status::NothingTodo);
}
