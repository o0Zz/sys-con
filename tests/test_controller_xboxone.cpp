#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Controllers/XboxOneController.h"
#include "mocks/Logger.h"
#include "mocks/Device.h"
#include "mocks/USBInterface.h"
#include "mocks/USBEndpoint.h"
#include <array>
#include <cstring>

MATCHER_P2(BufferMatches, expected, size, "Matches buffer content")
{
    return std::memcmp(arg, expected, size) == 0;
}

TEST(Controller, test_xboxone_unknown_controller_init)
{
    ControllerConfig config;

    auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
    auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
    EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(CONTROLLER_STATUS_SUCCESS));
    EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(CONTROLLER_STATUS_SUCCESS));

    uint8_t power_on[] = {0x05, 0x20, 0x01, 0x01, 0x00};
    EXPECT_CALL(*mockUSBEndpointOut, Write(BufferMatches(power_on, sizeof(power_on)), sizeof(power_on)))
        .Times(1)
        .WillOnce(testing::Return(CONTROLLER_STATUS_SUCCESS));

    XboxOneController controller(std::make_unique<MockDevice>(0x1234, 0x1234, std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut))), config, std::make_unique<MockLogger>());
    controller.Initialize();
}
/*
TEST(Controller, test_xboxone_pdp_controller_init)
{
    ControllerConfig config;

    auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
    auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
    EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(CONTROLLER_STATUS_SUCCESS));
    EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(CONTROLLER_STATUS_SUCCESS));

    uint8_t buffer1[] = {0x05, 0x20, 0x01, 0x01, 0x00};
    uint8_t buffer2[] = {0x05, 0x06, 0x07, 0x08}; // Second expected buffer
    uint8_t buffer3[] = {0x09, 0x0A, 0x0B, 0x0C}; // Third expected buffer

    testing::Sequence seq;

    EXPECT_CALL(*mockUSBEndpointOut, Write(BufferMatches(buffer1, sizeof(buffer1)), sizeof(buffer1)))
        .InSequence(seq)
        .WillOnce(testing::Return(CONTROLLER_STATUS_SUCCESS));

    EXPECT_CALL(*mockUSBEndpointOut, Write(testing::ElementsAreArray(buffer2, sizeof(buffer2)), sizeof(buffer2)))
        .InSequence(seq)
        .WillOnce(testing::Return(CONTROLLER_STATUS_SUCCESS));

    EXPECT_CALL(*mockUSBEndpointOut, Write(testing::ElementsAreArray(buffer3, sizeof(buffer3)), sizeof(buffer3)))
        .InSequence(seq)
        .WillOnce(testing::Return(CONTROLLER_STATUS_SUCCESS));

    XboxOneController controller(std::make_unique<MockDevice>(0x1234, 0x1234, std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut))), config, std::make_unique<MockLogger>());
    controller.Initialize();
}
*/
TEST(Controller, test_xboxone_s_init)
{
    ControllerConfig config;

    auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
    auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
    EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(CONTROLLER_STATUS_SUCCESS));
    EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(CONTROLLER_STATUS_SUCCESS));

    EXPECT_CALL(*mockUSBEndpointOut, Write(testing::_, testing::_))
        .Times(3)
        .WillRepeatedly(testing::Return(CONTROLLER_STATUS_SUCCESS));

    XboxOneController controller(std::make_unique<MockDevice>(0x045e, 0x0b00, std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut))), config, std::make_unique<MockLogger>());
    controller.Initialize();
}