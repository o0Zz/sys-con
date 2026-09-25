#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "drivers/BaseController.h"
#include "config_handler.h"
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
    class TemplateController : public BaseController
    {
    public:
        TemplateController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
            : BaseController(std::move(device), config, std::move(logger)) {}

        Status ParseData(uint8_t *, size_t, RawInputData *, uint16_t *) override { return Status::Success; }
    };

    ControllerConfig ConfigWithTemplate(const char *tmpl)
    {
        ControllerConfig config;
        std::string value(tmpl);
        EXPECT_TRUE(::syscon::config::ParseRumbleTemplate(value.c_str(), &config.rumble));
        return config;
    }

    std::unique_ptr<TemplateController> MakeController(const ControllerConfig &config, MockUSBEndpoint **outEndpoint)
    {
        auto mockUSBEndpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
        auto mockUSBEndpointOut = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_OUT);
        *outEndpoint = mockUSBEndpointOut.get();
        EXPECT_CALL(*mockUSBEndpointIn, Open).WillOnce(testing::Return(Status::Success));
        EXPECT_CALL(*mockUSBEndpointOut, Open).WillOnce(testing::Return(Status::Success));

        auto mockUSBInterface = std::make_unique<MockUSBInterface>(std::move(mockUSBEndpointIn), std::move(mockUSBEndpointOut));
        EXPECT_CALL(*mockUSBInterface, Open).WillOnce(testing::Return(Status::Success));

        auto controller = std::make_unique<TemplateController>(std::make_unique<MockDevice>(0x1234, 0x5678, std::move(mockUSBInterface)), config, std::make_unique<MockLogger>());
        EXPECT_EQ(controller->Initialize(), Status::Success);
        return controller;
    }
} // namespace

TEST(RumbleTemplate, test_parses_single_byte_fields)
{
    ControllerRumbleConfig rumble;

    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("00 08 00 LL RR 00 00 00", &rumble));

    EXPECT_EQ(rumble.packetSize, 8);
    EXPECT_EQ(rumble.packet[1], 0x08);
    EXPECT_EQ(rumble.low.offset, 3);
    EXPECT_EQ(rumble.low.size, 1);
    EXPECT_EQ(rumble.high.offset, 4);
    EXPECT_EQ(rumble.high.size, 1);
    EXPECT_FALSE(rumble.low.littleEndian);
}

TEST(RumbleTemplate, test_parses_two_byte_fields_and_endianness)
{
    ControllerRumbleConfig rumble;

    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("EBllllRRRR", &rumble));

    EXPECT_EQ(rumble.packetSize, 5);
    EXPECT_EQ(rumble.packet[0], 0xEB);
    EXPECT_EQ(rumble.low.offset, 1);
    EXPECT_EQ(rumble.low.size, 2);
    EXPECT_TRUE(rumble.low.littleEndian);
    EXPECT_EQ(rumble.high.offset, 3);
    EXPECT_EQ(rumble.high.size, 2);
    EXPECT_FALSE(rumble.high.littleEndian);
}

TEST(RumbleTemplate, test_repeat_expands_the_previous_byte)
{
    ControllerRumbleConfig rumble;

    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("05 01 00 00 RR LL 00*26", &rumble));

    EXPECT_EQ(rumble.packetSize, 32);
    EXPECT_EQ(rumble.packet[0], 0x05);
    EXPECT_EQ(rumble.packet[1], 0x01);
    EXPECT_EQ(rumble.high.offset, 4);
    EXPECT_EQ(rumble.low.offset, 5);
    for (uint8_t i = 6; i < rumble.packetSize; i++)
        EXPECT_EQ(rumble.packet[i], 0x00);
}

TEST(RumbleTemplate, test_repeat_keeps_the_repeated_value)
{
    ControllerRumbleConfig rumble;

    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("LL AB*3 RR", &rumble));

    EXPECT_EQ(rumble.packetSize, 5);
    EXPECT_EQ(rumble.packet[1], 0xAB);
    EXPECT_EQ(rumble.packet[2], 0xAB);
    EXPECT_EQ(rumble.packet[3], 0xAB);
    EXPECT_EQ(rumble.high.offset, 4);
}

TEST(RumbleTemplate, test_shipped_templates_are_the_documented_reports)
{
    ControllerRumbleConfig dualsense;
    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("02 03 00 RR LL 00*58", &dualsense));
    EXPECT_EQ(dualsense.packetSize, 63);
    EXPECT_EQ(dualsense.packet[0], 0x02);
    EXPECT_EQ(dualsense.packet[1], 0x03);
    EXPECT_EQ(dualsense.high.offset, 3);
    EXPECT_EQ(dualsense.low.offset, 4);

    ControllerRumbleConfig dualshock4;
    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("05 01 00 00 RR LL 00*26", &dualshock4));
    EXPECT_EQ(dualshock4.packetSize, 32);

    ControllerRumbleConfig mayflash;
    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("RR LL", &mayflash));
    EXPECT_EQ(mayflash.packetSize, 2);
    EXPECT_EQ(mayflash.high.offset, 0);
    EXPECT_EQ(mayflash.low.offset, 1);

    ControllerRumbleConfig stadia;
    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("05 llll rrrr", &stadia));
    EXPECT_EQ(stadia.packetSize, 5);
    EXPECT_EQ(stadia.low.offset, 1);
    EXPECT_EQ(stadia.low.size, 2);
    EXPECT_TRUE(stadia.low.littleEndian);
    EXPECT_EQ(stadia.high.offset, 3);
    EXPECT_TRUE(stadia.high.littleEndian);
}

TEST(RumbleTemplate, test_rejects_malformed_templates)
{
    ControllerRumbleConfig rumble;

    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 08 00 00", &rumble));       // no placeholder
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 0 LL", &rumble));           // odd hex digit count
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 L RR", &rumble));           // half a byte of placeholder
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 LLLLLL", &rumble));         // three byte motor
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 LL LL", &rumble));          // same motor twice
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 LL ZZ", &rumble));          // not hex, not a placeholder
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("*4 LL", &rumble));             // nothing to repeat
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 LL*2", &rumble));           // a placeholder is not a byte
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 LL 00*x", &rumble));        // no count
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 LL 00*200", &rumble));      // longer than the packet
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 LL 00*", &rumble));         // count missing at end of string
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 LL 00* 4", &rumble));       // count is not glued to the star
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 LL 00*+4", &rumble));       // signed count
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 LL AB*2*3", &rumble));      // a repeat is not a byte
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("Ll RR", &rumble));             // mixed case in one run
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("lLLL RR", &rumble));           // mixed case in one run
    EXPECT_FALSE(rumble.IsValid());
}

TEST(RumbleTemplate, test_packet_cannot_exceed_the_buffer)
{
    ControllerRumbleConfig rumble;

    // A placeholder run is the one thing that is not a byte-at-a-time append, so it is the
    // one that can walk past the end.
    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("00*62 LLLL", &rumble));
    EXPECT_EQ(rumble.packetSize, MAX_RUMBLE_PACKET_SIZE);

    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00*63 LLLL", &rumble));
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00*62 LLLL RR", &rumble));
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00*63 LL 00*2", &rumble));

    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("LL AB*63", &rumble));
    EXPECT_EQ(rumble.packetSize, MAX_RUMBLE_PACKET_SIZE);
}

TEST(RumbleTemplate, test_repeat_count_cannot_overflow)
{
    ControllerRumbleConfig rumble;

    // strtol saturates to LONG_MAX, which is 64 bit on the console: the length check must not
    // do arithmetic with the raw count.
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("AB CD*99999999999999999999 LL", &rumble));
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("AB*99999999999999999999 LL", &rumble));
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("AB CD*65 LL", &rumble));
}

TEST(RumbleTemplate, test_rejected_template_leaves_the_previous_one_intact)
{
    ControllerRumbleConfig rumble;

    EXPECT_TRUE(::syscon::config::ParseRumbleTemplate("05 01 00 00 RR LL 00*26", &rumble));

    const ControllerRumbleConfig good = rumble;

    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00 ZZ LL", &rumble));
    EXPECT_FALSE(::syscon::config::ParseRumbleTemplate("00*63 LLLL", &rumble));

    EXPECT_EQ(rumble.packetSize, good.packetSize);
    EXPECT_EQ(rumble.low.offset, good.low.offset);
    EXPECT_EQ(rumble.high.offset, good.high.offset);
    EXPECT_EQ(std::memcmp(rumble.packet.data(), good.packet.data(), good.packetSize), 0);
}

TEST(RumbleTemplate, test_controller_without_template_has_no_rumble)
{
    ControllerConfig config;
    TemplateController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    EXPECT_FALSE(controller.Support(SUPPORTS_RUMBLE));
    EXPECT_EQ(controller.SetRumble(0, RumbleValue{.left = {.amp_low = 1.0f, .amp_high = 1.0f}}), Status::NotImplemented);
}

TEST(RumbleTemplate, test_controller_writes_the_template)
{
    MockUSBEndpoint *outEndpoint = nullptr;
    auto controller = MakeController(ConfigWithTemplate("00 08 00 LL RR 00 00 00"), &outEndpoint);

    EXPECT_TRUE(controller->Support(SUPPORTS_RUMBLE));

    uint8_t expected[]{0x00, 0x08, 0x00, 0xff, 0x7f, 0x00, 0x00, 0x00};
    EXPECT_CALL(*outEndpoint, Write(BufferMatches(expected, sizeof(expected)), sizeof(expected)))
        .Times(1)
        .WillOnce(testing::Return(Status::Success));

    EXPECT_EQ(controller->SetRumble(0, RumbleValue{.left = {.amp_low = 1.0f, .amp_high = 0.5f}}), Status::Success);
}

TEST(RumbleTemplate, test_controller_writes_two_byte_fields)
{
    MockUSBEndpoint *outEndpoint = nullptr;
    auto controller = MakeController(ConfigWithTemplate("EB llll RRRR"), &outEndpoint);

    // Half scale in both fields, so the byte order of each is what the bytes prove.
    uint8_t expected[]{0xEB, 0xff, 0x7f, 0x7f, 0xff};
    EXPECT_CALL(*outEndpoint, Write(BufferMatches(expected, sizeof(expected)), sizeof(expected)))
        .Times(1)
        .WillOnce(testing::Return(Status::Success));

    EXPECT_EQ(controller->SetRumble(0, RumbleValue{.left = {.amp_low = 0.5f, .amp_high = 0.5f}}), Status::Success);
}

TEST(RumbleTemplate, test_controller_without_output_endpoint)
{
    ControllerConfig config = ConfigWithTemplate("00 08 00 LL RR 00 00 00");
    TemplateController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    // A template is not a motor: without somewhere to send it, the pad has no rumble.
    EXPECT_FALSE(controller.Support(SUPPORTS_RUMBLE));
    EXPECT_EQ(controller.SetRumble(0, RumbleValue{.left = {.amp_low = 1.0f, .amp_high = 1.0f}}), Status::InvalidIndex);
}
