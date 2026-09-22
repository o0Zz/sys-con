#include <gtest/gtest.h>
#include "drivers/SwitchController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"
#include "mocks/USBEndpoint.h"
#include "mocks/USBInterface.h"
#include <cstring>

using namespace controllerlib;

TEST(Controller, test_switch_button1)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;
    SwitchController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[64] = {0x30, 0x0D, 0x91, 0x01, 0x80, 0x00, 0xB9, 0x77, 0x7D, 0xDB, 0xF7, 0x7B, 0x09, 0x00, 0x00, 0x00};
    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::Success);

    EXPECT_TRUE(rawData.buttons[1]);
}

TEST(Controller, test_switch_lstick_left)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    SwitchController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[64] = {0x30, 0xFC, 0x91, 0x00, 0x80, 0x00, 0xC6, 0xB1, 0x72, 0xE5, 0xE7, 0x79, 0x03, 0x00, 0x00, 0x00};
    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::Success);

    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::X], -1.0f);
}

TEST(Controller, test_switch_init_handshake_writes_before_reading)
{
    ControllerConfig config;

    auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
    auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
    EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(Status::Success));
    EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(Status::Success));

    testing::Sequence seq;
    EXPECT_CALL(*mockUSBEndpointOut, Write(testing::_, testing::_))
        .InSequence(seq)
        .WillOnce(testing::Invoke([](const uint8_t *inBuffer, size_t) {
            EXPECT_EQ(inBuffer[0], 0x80);
            EXPECT_EQ(inBuffer[1], 0x02);
            return Status::Success;
        }));
    EXPECT_CALL(*mockUSBEndpointIn, Read(testing::_, testing::_, testing::_))
        .InSequence(seq)
        .WillOnce(testing::Return(Status::Timeout));
    EXPECT_CALL(*mockUSBEndpointOut, Write(testing::_, testing::_))
        .InSequence(seq)
        .WillOnce(testing::Invoke([](const uint8_t *inBuffer, size_t) {
            EXPECT_EQ(inBuffer[0], 0x80);
            EXPECT_EQ(inBuffer[1], 0x04);
            return Status::Success;
        }));
    EXPECT_CALL(*mockUSBEndpointOut, Write(testing::_, 12))
        .InSequence(seq)
        .WillOnce(testing::Invoke([](const uint8_t *inBuffer, size_t) {
            EXPECT_EQ(inBuffer[0], 0x01);
            EXPECT_EQ(inBuffer[10], 0x48);
            EXPECT_EQ(inBuffer[11], 0x01);
            return Status::Success;
        }));

    SwitchController controller(std::make_unique<MockDevice>(0x057e, 0x2009, std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut))), config, std::make_unique<MockLogger>());
    EXPECT_EQ(controller.Initialize(), Status::Success);
}
