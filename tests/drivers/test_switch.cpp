#include <gtest/gtest.h>
#include "drivers/SwitchController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"
#include "mocks/USBEndpoint.h"
#include "mocks/USBInterface.h"
#include <cstring>

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
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

TEST(Controller, test_switch_init_flush_stops_on_streaming_device)
{
    ControllerConfig config;

    auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
    auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
    EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(Status::Success));
    EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(Status::Success));

    // HOJA based Pro Controllers stream 0x30 reports before the handshake, a read never times out.
    // The guard keeps an unbounded flush loop from hanging the test suite instead of failing it.
    size_t readCount = 0;
    EXPECT_CALL(*mockUSBEndpointIn, Read(testing::_, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Invoke([&readCount](uint8_t *outBuffer, size_t *bufferSizeInOut, uint64_t) {
            if (++readCount > 32)
                return Status::Timeout;

            *bufferSizeInOut = sizeof(SwitchButtonData);
            memset(outBuffer, 0x00, *bufferSizeInOut);
            outBuffer[0] = 0x30;
            return Status::Success;
        }));

    EXPECT_CALL(*mockUSBEndpointOut, Write(testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(Status::Success));

    SwitchController controller(std::make_unique<MockDevice>(0x057e, 0x2009, std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut))), config, std::make_unique<MockLogger>());
    EXPECT_EQ(controller.Initialize(), Status::Success);
}

TEST(Controller, test_switch_init_flush_stops_on_timeout)
{
    ControllerConfig config;

    auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
    auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
    EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(Status::Success));
    EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(Status::Success));

    testing::Sequence seq;
    EXPECT_CALL(*mockUSBEndpointIn, Read(testing::_, testing::_, testing::_))
        .InSequence(seq)
        .WillOnce(testing::Return(Status::Timeout));
    EXPECT_CALL(*mockUSBEndpointIn, Read(testing::_, testing::_, testing::_))
        .InSequence(seq)
        .WillOnce(testing::Return(Status::Success));

    EXPECT_CALL(*mockUSBEndpointOut, Write(testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(Status::Success));

    SwitchController controller(std::make_unique<MockDevice>(0x057e, 0x2009, std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut))), config, std::make_unique<MockLogger>());
    EXPECT_EQ(controller.Initialize(), Status::Success);
}
