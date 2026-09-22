/*
    The network pad, end to end against the real shipped config.ini.

    test_network_controller.cpp covers ParseData in isolation, which only proves the packet
    reaches the *pin* space. What actually decides whether pressing A on the PC presses A on
    the console is the [network] profile in src/app/config.ini: its pin numbers have to match
    the identity mapping NetworkController writes, its ZL/ZR have to override [default]'s analog
    trigger bindings, and its deadzones have to be zeroed or the first 20% of every axis is
    silently discarded. None of that is visible from either file on its own.

    Everything here is host-buildable - NetworkController, MapRawInputToNormalized and the
    config parser all are - so the profile is checked on every CI run, even though the
    transport it feeds (src/platform/UdpDevice.cpp) is device-only.
*/
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "drivers/NetworkController.h"
#include "config_handler.h"
#include "StdFileManager.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"
#include "mocks/USBEndpoint.h"
#include "mocks/USBInterface.h"

#include <cstring>

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
using namespace controllerlib;

using testing::_;
using testing::Invoke;
using testing::Return;

#ifndef CONFIG_FULLPATH_PROJECT
    #define CONFIG_FULLPATH_PROJECT "../../src/app/config.ini"
#endif

namespace
{
    // The VID/PID src/platform/UdpDevice.h reports, and the section config.ini ships.
    constexpr uint16_t kNetworkVendorId = 0xFFFF;
    constexpr uint16_t kNetworkProductId = 0x0001;

    ControllerConfig LoadNetworkProfile()
    {
        ControllerConfig config;
        syscon::StdFileManager fileManager;
        ::syscon::config::Initialize(fileManager);
        ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, kNetworkVendorId, kNetworkProductId, false, "network");
        return config;
    }

    std::vector<uint8_t> MakePacket(uint32_t buttons, int16_t lx = 0, int16_t ly = 0, int16_t rx = 0, int16_t ry = 0)
    {
        NetworkPadReport report{};
        report.magic = NetworkPadReportMagic;
        report.version = NetworkPadReportVersion;
        report.connected = 1;
        report.buttons = buttons;
        report.stick_left_x = lx;
        report.stick_left_y = ly;
        report.stick_right_x = rx;
        report.stick_right_y = ry;

        std::vector<uint8_t> bytes(sizeof(report));
        memcpy(bytes.data(), &report, sizeof(report));
        return bytes;
    }

    constexpr uint32_t Bit(GamepadButton button)
    {
        return 1u << static_cast<uint8_t>(button);
    }

    // Runs one full ReadInput() pass: endpoint -> ParseData -> MapRawInputToNormalized.
    // The endpoint yields the packet once, then reports "nothing more queued" so the drain
    // loop in ReadEndpointLatest terminates.
    class NetworkPadUnderTest
    {
    public:
        explicit NetworkPadUnderTest(const ControllerConfig &config)
            : m_descriptor{}
        {
            m_descriptor.bEndpointAddress = 0x81;
            m_descriptor.wMaxPacketSize = 64;

            auto endpointIn = std::make_unique<MockUSBEndpoint>(IUSBEndpoint::USB_ENDPOINT_IN);
            m_endpoint = endpointIn.get();

            EXPECT_CALL(*m_endpoint, Open(_)).WillRepeatedly(Return(Status::Success));
            EXPECT_CALL(*m_endpoint, GetDescriptor()).WillRepeatedly(Return(&m_descriptor));

            auto interface = std::make_unique<MockUSBInterface>(std::move(endpointIn), nullptr);
            EXPECT_CALL(*interface, Open()).WillRepeatedly(Return(Status::Success));

            m_controller = std::make_unique<NetworkController>(
                std::make_unique<MockDevice>(kNetworkVendorId, kNetworkProductId, std::move(interface)),
                config, std::make_unique<MockLogger>());

            EXPECT_EQ(m_controller->Initialize(), Status::Success);
        }

        NormalizedButtonData Feed(const std::vector<uint8_t> &packet)
        {
            EXPECT_CALL(*m_endpoint, Read(_, _, _))
                .WillOnce(Invoke([packet](uint8_t *out, size_t *size, uint64_t)
                                 {
                        memcpy(out, packet.data(), packet.size());
                        *size = packet.size();
                        return Status::Success; }))
                .WillRepeatedly(Return(Status::Timeout));

            NormalizedButtonData data{};
            uint16_t input_idx = 0;
            EXPECT_EQ(m_controller->ReadInput(&data, &input_idx, 0), Status::Success);
            return data;
        }

    private:
        IUSBEndpoint::EndpointDescriptor m_descriptor;
        MockUSBEndpoint *m_endpoint = nullptr;
        std::unique_ptr<NetworkController> m_controller;
    };
} // namespace

TEST(NetworkProfile, test_config_ini_ships_a_network_profile)
{
    ControllerConfig config = LoadNetworkProfile();

    // If this fails, [ffff-0001] or [network] went missing from config.ini and the pad would
    // silently fall back to [default], where every pin is 0 and no button does anything.
    EXPECT_EQ(config.driver, "network");
    EXPECT_EQ(config.profile, "network");
}

TEST(NetworkProfile, test_profile_pins_match_the_identity_mapping)
{
    ControllerConfig config = LoadNetworkProfile();

    // NetworkController::ParseData writes bit N to pin N, so the profile has to read pin N back
    // for button N. Spot-checking one per group: face, shoulder, trigger, d-pad, system.
    EXPECT_EQ(config.buttonsPin[GamepadButton::A][0], static_cast<uint8_t>(GamepadButton::A));
    EXPECT_EQ(config.buttonsPin[GamepadButton::B][0], static_cast<uint8_t>(GamepadButton::B));
    EXPECT_EQ(config.buttonsPin[GamepadButton::L][0], static_cast<uint8_t>(GamepadButton::L));
    EXPECT_EQ(config.buttonsPin[GamepadButton::ZR][0], static_cast<uint8_t>(GamepadButton::ZR));
    EXPECT_EQ(config.buttonsPin[GamepadButton::DPAD_LEFT][0], static_cast<uint8_t>(GamepadButton::DPAD_LEFT));
    EXPECT_EQ(config.buttonsPin[GamepadButton::HOME][0], static_cast<uint8_t>(GamepadButton::HOME));
}

TEST(NetworkProfile, test_profile_clears_the_stick_deadzones)
{
    ControllerConfig config = LoadNetworkProfile();

    // [default] sets these to 20, which would quietly swallow every small stick movement a
    // test asks for. The sender already says exactly where the stick is.
    EXPECT_EQ(config.analogDeadzonePercent[AnalogAxis::X], 0);
    EXPECT_EQ(config.analogDeadzonePercent[AnalogAxis::Y], 0);
    EXPECT_EQ(config.analogDeadzonePercent[AnalogAxis::Z], 0);
    EXPECT_EQ(config.analogDeadzonePercent[AnalogAxis::Rz], 0);
}

TEST(NetworkProfile, test_pressing_a_button_reaches_the_normalized_output)
{
    NetworkPadUnderTest pad(LoadNetworkProfile());

    NormalizedButtonData data = pad.Feed(MakePacket(Bit(GamepadButton::A) | Bit(GamepadButton::DPAD_UP)));

    EXPECT_TRUE(data.buttons[GamepadButton::A]);
    EXPECT_TRUE(data.buttons[GamepadButton::DPAD_UP]);
    EXPECT_FALSE(data.buttons[GamepadButton::B]);
    EXPECT_FALSE(data.buttons[GamepadButton::HOME]);
}

TEST(NetworkProfile, test_triggers_are_digital_not_analog)
{
    // [default] binds ZL/ZR to the analog Rx/Ry axes, which this pad never reports. The
    // profile has to override that with pins, or ZL/ZR would be permanently unpressable.
    NetworkPadUnderTest pad(LoadNetworkProfile());

    NormalizedButtonData data = pad.Feed(MakePacket(Bit(GamepadButton::ZL) | Bit(GamepadButton::ZR)));

    EXPECT_TRUE(data.buttons[GamepadButton::ZL]);
    EXPECT_TRUE(data.buttons[GamepadButton::ZR]);
}

TEST(NetworkProfile, test_sticks_reach_the_normalized_output_undamped)
{
    NetworkPadUnderTest pad(LoadNetworkProfile());

    // Full up on the left stick, full left on the right stick.
    NormalizedButtonData data = pad.Feed(MakePacket(0, 0, 32767, -32768, 0));

    EXPECT_FLOAT_EQ(data.sticks[0].axis_x, 0.0f);
    EXPECT_FLOAT_EQ(data.sticks[0].axis_y, 1.0f);
    EXPECT_FLOAT_EQ(data.sticks[1].axis_x, -1.0f);
    EXPECT_FLOAT_EQ(data.sticks[1].axis_y, 0.0f);
}

TEST(NetworkProfile, test_small_stick_movement_is_not_swallowed)
{
    NetworkPadUnderTest pad(LoadNetworkProfile());

    // 10% of full deflection - inside [default]'s 20% deadzone, so this is what catches a
    // profile that forgot to clear it.
    NormalizedButtonData data = pad.Feed(MakePacket(0, 3277, 0, 0, 0));

    EXPECT_GT(data.sticks[0].axis_x, 0.05f);
    EXPECT_LT(data.sticks[0].axis_x, 0.15f);
}
