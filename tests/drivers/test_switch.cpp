#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "drivers/SwitchController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"
#include "mocks/USBEndpoint.h"
#include "mocks/USBInterface.h"
#include <cstring>

using namespace controllerlib;

MATCHER_P2(BufferMatches, expected, size, "Matches buffer content")
{
    return std::memcmp(arg, expected, size) == 0;
}

namespace
{
    std::unique_ptr<SwitchController> MakeInitializedController(const ControllerConfig &config, MockUSBEndpoint **outEndpoint)
    {
        auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
        auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
        *outEndpoint = mockUSBEndpointOut.get();

        EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(Status::Success));
        EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(Status::Success));
        EXPECT_CALL(*mockUSBEndpointIn, Read(testing::_, testing::_, testing::_)).WillRepeatedly(testing::Return(Status::Timeout));
        EXPECT_CALL(*mockUSBEndpointOut, Write(testing::_, testing::_)).Times(3).WillRepeatedly(testing::Return(Status::Success));

        auto controller = std::make_unique<SwitchController>(std::make_unique<MockDevice>(0x057e, 0x2009, std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut))), config, std::make_unique<MockLogger>());
        EXPECT_EQ(controller->Initialize(), Status::Success);
        return controller;
    }
} // namespace

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
    // The enable-vibration subcommand must not overtake the handshake reply.
    EXPECT_CALL(*mockUSBEndpointIn, Read(testing::_, testing::_, testing::_))
        .InSequence(seq)
        .WillOnce(testing::Return(Status::Timeout));
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

TEST(Controller, test_switch_rumble_full_scale)
{
    ControllerConfig config;
    MockUSBEndpoint *outEndpoint = nullptr;
    auto controller = MakeInitializedController(config, &outEndpoint);

    EXPECT_TRUE(controller->Support(SUPPORTS_RUMBLE));

    // Report 0x10, packet counter, then the encoded pair per side. Full scale on the low
    // frequency band is the documented 00 C9 40 72, idle is 00 01 40 40.
    uint8_t expected[]{0x10, 0x01, 0x00, 0xC9, 0x40, 0x72, 0x00, 0x01, 0x40, 0x40};

    EXPECT_CALL(*outEndpoint, Write(BufferMatches(expected, sizeof(expected)), sizeof(expected)))
        .Times(1)
        .WillOnce(testing::Return(Status::Success));

    EXPECT_EQ(controller->SetRumble(0, 0.0f, 1.0f), Status::Success);
}

TEST(Controller, test_switch_rumble_follows_the_amplitude_table)
{
    ControllerConfig config;
    MockUSBEndpoint *outEndpoint = nullptr;
    auto controller = MakeInitializedController(config, &outEndpoint);

    /*
        Half amplitude is 501 thousandths, which is step 68 of the pad's own amplitude table -
        not step 50. A linear step would encode 0x65/0x40/0x59 and feel far weaker than asked.
    */
    uint8_t expected[]{0x10, 0x01, 0x00, 0x89, 0x40, 0x62, 0x00, 0x01, 0x40, 0x40};

    EXPECT_CALL(*outEndpoint, Write(BufferMatches(expected, sizeof(expected)), sizeof(expected)))
        .Times(1)
        .WillOnce(testing::Return(Status::Success));

    EXPECT_EQ(controller->SetRumble(0, 0.0f, 0.5f), Status::Success);
}

TEST(Controller, test_switch_rumble_without_output_endpoint)
{
    ControllerConfig config;
    SwitchController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    EXPECT_EQ(controller.SetRumble(0, 1.0f, 1.0f), Status::InvalidEndpoint);
    EXPECT_EQ(controller.SetRumble(1, 1.0f, 1.0f), Status::InvalidIndex);
}
