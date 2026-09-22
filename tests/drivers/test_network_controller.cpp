#include <gtest/gtest.h>
#include "drivers/NetworkController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"
#include <cstring>

using namespace controllerlib;

namespace
{
    NetworkPadReport MakeReport()
    {
        NetworkPadReport report{};
        report.magic = NetworkPadReportMagic;
        report.version = NetworkPadReportVersion;
        report.pad_index = 0;
        report.connected = 1;
        return report;
    }

    // The transport hands ParseData a byte buffer, so the tests go through one too rather
    // than passing a struct pointer that would hide a layout mistake.
    void Serialize(const NetworkPadReport &report, uint8_t *out)
    {
        memcpy(out, &report, sizeof(report));
    }
} // namespace

TEST(Controller, test_network_report_is_20_bytes)
{
    // The wire format is shared with tools/networkpad.py, which packs it as '<IBBBBIhhhh'.
    // If this changes, that script changes with it.
    EXPECT_EQ(sizeof(NetworkPadReport), 20u);
}

TEST(Controller, test_network_controller_buttons_map_to_identical_pins)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;
    uint8_t buffer[sizeof(NetworkPadReport)];

    NetworkController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    NetworkPadReport report = MakeReport();
    report.buttons = (1u << static_cast<uint8_t>(GamepadButton::A)) |
                     (1u << static_cast<uint8_t>(GamepadButton::DPAD_LEFT)) |
                     (1u << static_cast<uint8_t>(GamepadButton::HOME));
    Serialize(report, buffer);

    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::Success);

    EXPECT_TRUE(rawData.buttons[static_cast<uint8_t>(GamepadButton::A)]);
    EXPECT_TRUE(rawData.buttons[static_cast<uint8_t>(GamepadButton::DPAD_LEFT)]);
    EXPECT_TRUE(rawData.buttons[static_cast<uint8_t>(GamepadButton::HOME)]);

    EXPECT_FALSE(rawData.buttons[static_cast<uint8_t>(GamepadButton::B)]);
    EXPECT_FALSE(rawData.buttons[static_cast<uint8_t>(GamepadButton::DPAD_RIGHT)]);

    // Bit 0 is GamepadButton::NONE, which is also PinId::Unmapped: reading it must stay false
    // or every unmapped button in the config would read as pressed.
    EXPECT_FALSE(rawData.buttons[PinId::Unmapped]);
}

TEST(Controller, test_network_controller_bit_zero_never_sets_the_unmapped_pin)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;
    uint8_t buffer[sizeof(NetworkPadReport)];

    NetworkController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    NetworkPadReport report = MakeReport();
    report.buttons = 0xFFFFFFFFu; // including bit 0, which a careless sender may well set
    Serialize(report, buffer);

    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::Success);

    EXPECT_FALSE(rawData.buttons[PinId::Unmapped]);
    EXPECT_TRUE(rawData.buttons[static_cast<uint8_t>(GamepadButton::X)]);
    EXPECT_TRUE(rawData.buttons[static_cast<uint8_t>(GamepadButton::CAPTURE)]);
}

TEST(Controller, test_network_controller_sticks_span_the_full_range)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;
    uint8_t buffer[sizeof(NetworkPadReport)];

    NetworkController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    NetworkPadReport report = MakeReport();
    report.stick_left_x = -32768;
    report.stick_left_y = 32767;
    report.stick_right_x = 0;
    report.stick_right_y = -32768;
    Serialize(report, buffer);

    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::Success);

    // Left stick is X/Y and right stick Z/Rz, matching the lstick_*/rstick_* bindings that
    // [default] already declares in config.ini.
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::X], -1.0f);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Y], 1.0f);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Z], 0.0f);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Rz], -1.0f);
}

TEST(Controller, test_network_controller_rejects_bad_magic)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;
    uint8_t buffer[sizeof(NetworkPadReport)];

    NetworkController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    NetworkPadReport report = MakeReport();
    report.magic = 0xDEADBEEF;
    Serialize(report, buffer);

    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::UnexpectedData);
    EXPECT_FALSE(controller.IsControllerConnected(0));
}

TEST(Controller, test_network_controller_rejects_unknown_version)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;
    uint8_t buffer[sizeof(NetworkPadReport)];

    NetworkController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    NetworkPadReport report = MakeReport();
    report.version = NetworkPadReportVersion + 1;
    Serialize(report, buffer);

    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::UnexpectedData);
}

TEST(Controller, test_network_controller_rejects_short_datagram)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;
    uint8_t buffer[sizeof(NetworkPadReport)];

    NetworkController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    Serialize(MakeReport(), buffer);

    EXPECT_EQ(controller.ParseData(buffer, sizeof(NetworkPadReport) - 1, &rawData, &input_idx), Status::UnexpectedData);
}

TEST(Controller, test_network_controller_starts_disconnected_and_tracks_the_report)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;
    uint8_t buffer[sizeof(NetworkPadReport)];

    NetworkController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    // A phantom pad at boot is exactly what starting disconnected avoids.
    EXPECT_FALSE(controller.IsControllerConnected(0));

    NetworkPadReport report = MakeReport();
    Serialize(report, buffer);
    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::Success);
    EXPECT_TRUE(controller.IsControllerConnected(0));

    // connected=0 is how a test simulates an unplug.
    report.connected = 0;
    Serialize(report, buffer);
    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::Success);
    EXPECT_FALSE(controller.IsControllerConnected(0));
}

TEST(Controller, test_network_controller_reports_no_rumble)
{
    ControllerConfig config;
    NetworkController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    EXPECT_FALSE(controller.Support(SUPPORTS_RUMBLE));
    EXPECT_EQ(controller.SetRumble(0, 1.0f, 1.0f), Status::NotImplemented);
}

TEST(Controller, test_network_controller_decodes_a_packet_from_networkpad_py)
{
    /*
        The exact 20 bytes tools/networkpad.py emits for "A held, left stick pushed fully up",
        captured from its struct.pack('<IBBBBIhhhh', ...). The script and NetworkPadReport have
        to agree on every offset, and nothing else in the build would notice if they stopped:
        the sender lives outside the C++ world entirely.
    */
    const uint8_t packet[] = {
        0x53, 0x43, 0x4E, 0x50, // magic 'SCNP'
        0x01,                   // version
        0x00,                   // pad_index
        0x01,                   // connected
        0x00,                   // reserved
        0x04, 0x00, 0x00, 0x00, // buttons: bit 2 == GamepadButton::A
        0x00, 0x00,             // stick_left_x = 0
        0xFF, 0x7F,             // stick_left_y = 32767
        0x00, 0x00,             // stick_right_x = 0
        0x00, 0x00,             // stick_right_y = 0
    };
    ASSERT_EQ(sizeof(packet), sizeof(NetworkPadReport));

    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    NetworkController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    uint8_t buffer[sizeof(NetworkPadReport)];
    memcpy(buffer, packet, sizeof(packet));

    EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), Status::Success);

    EXPECT_TRUE(controller.IsControllerConnected(0));
    EXPECT_TRUE(rawData.buttons[static_cast<uint8_t>(GamepadButton::A)]);
    EXPECT_FALSE(rawData.buttons[static_cast<uint8_t>(GamepadButton::B)]);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::X], 0.0f);
    EXPECT_FLOAT_EQ(rawData.analog[AnalogAxis::Y], 1.0f);
}
