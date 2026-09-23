#include <gtest/gtest.h>
#include "drivers/BaseController.h"
#include "config_handler.h"
#include "StdFileManager.h"

using namespace controllerlib;

// CONFIG_FULLPATH_PROJECT is supplied by tests/CMakeLists.txt as an absolute path, so the
// tests work from any build directory. The fallback keeps a hand-run build working.
#ifndef CONFIG_FULLPATH_PROJECT
    #define CONFIG_FULLPATH_PROJECT "../../src/app/config.ini"
#endif

TEST(Configuration, test_load_config_unknown)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x0000, 0x0000, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "");
    EXPECT_EQ(config.profile, "");
    EXPECT_EQ(config.controllerType, ControllerType_Pro);
    EXPECT_EQ(config.buttonsPin[GamepadButton::X][0], 0);
    EXPECT_EQ(config.buttonsPin[GamepadButton::A][0], 0);
    EXPECT_EQ(config.buttonsPin[GamepadButton::B][0], 0);
    EXPECT_EQ(config.buttonsPin[GamepadButton::Y][0], 0);
}

TEST(Configuration, test_load_config_no_profile)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x054c, 0x0cda, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "");
    EXPECT_EQ(config.profile, "");
    EXPECT_EQ(config.controllerType, ControllerType_Pro);
    EXPECT_EQ(config.buttonsPin[GamepadButton::X][0], 1);
    EXPECT_EQ(config.buttonsPin[GamepadButton::A][0], 2);
    EXPECT_EQ(config.buttonsPin[GamepadButton::B][0], 3);
    EXPECT_EQ(config.buttonsPin[GamepadButton::Y][0], 4);
}

TEST(Configuration, test_load_config_with_profile_xboxone)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x045e, 0x02dd, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "xboxone");
    EXPECT_EQ(config.profile, "xboxone");
    EXPECT_EQ(config.controllerType, ControllerType_Pro);
    EXPECT_EQ(config.buttonsPin[GamepadButton::X][0], 4);
    EXPECT_EQ(config.buttonsPin[GamepadButton::A][0], 2);
    EXPECT_EQ(config.buttonsPin[GamepadButton::B][0], 1);
    EXPECT_EQ(config.buttonsPin[GamepadButton::Y][0], 3);
    EXPECT_EQ(config.simulateCombos[0].buttonSimulated, GamepadButton::CAPTURE);
    EXPECT_EQ(config.simulateCombos[1].buttonSimulated, GamepadButton::HOME);
    EXPECT_EQ(config.simulateCombos[2].buttonSimulated, GamepadButton::NONE);
}

TEST(Configuration, test_load_config_with_profile_wii)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x057e, 0x0337, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "wii");
    EXPECT_EQ(config.profile, "wii");
    EXPECT_EQ(config.buttonsPin[GamepadButton::ZL][0], 0);
}

TEST(Configuration, test_load_config_with_profile_sinput)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x2e8a, 0x10df, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "sinput");
    EXPECT_EQ(config.profile, "sinput");
    EXPECT_EQ(config.buttonsPin[GamepadButton::B][0], 1);
    EXPECT_EQ(config.buttonsPin[GamepadButton::A][0], 2);
    EXPECT_EQ(config.buttonsPin[GamepadButton::Y][0], 3);
    EXPECT_EQ(config.buttonsPin[GamepadButton::X][0], 4);

    // ZL/ZR take the digital bit and the analog trigger, so either kind of device works.
    EXPECT_EQ(config.buttonsPin[GamepadButton::ZL][0], 9);
    EXPECT_EQ(config.buttonsAnalog[GamepadButton::ZL].bind, AnalogAxis::Rx);
    EXPECT_EQ(config.buttonsPin[GamepadButton::ZR][0], 10);
    EXPECT_EQ(config.buttonsAnalog[GamepadButton::ZR].bind, AnalogAxis::Ry);
}
TEST(Configuration, test_load_config_vibration_template_dualshock4)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);

    // DualShock 4 v2. These assertions track the shipped config.ini and have to be updated
    // with it; the parser itself is covered in tests/core/test_rumble_template.cpp.
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x054c, 0x09cc, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.profile, "dualshock4");
    EXPECT_TRUE(config.rumble.IsValid());
    EXPECT_EQ(config.rumble.packetSize, 32);
    EXPECT_EQ(config.rumble.packet[0], 0x05);
    EXPECT_EQ(config.rumble.packet[0], 0x05);
    EXPECT_EQ(config.rumble.packet[1], 0x01);
    EXPECT_EQ(config.rumble.high.offset, 4);
    EXPECT_EQ(config.rumble.high.size, 1);
    EXPECT_EQ(config.rumble.low.offset, 5);
    EXPECT_EQ(config.rumble.low.size, 1);
}

TEST(Configuration, test_load_config_vibration_template_stadia)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);

    // Google Stadia, whose template sits in its own VID/PID section.
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x18d1, 0x9400, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_TRUE(config.rumble.IsValid());
    EXPECT_EQ(config.rumble.packetSize, 5);
    EXPECT_EQ(config.rumble.packet[0], 0x05);
    EXPECT_EQ(config.rumble.low.offset, 1);
    EXPECT_EQ(config.rumble.low.size, 2);
    EXPECT_TRUE(config.rumble.low.littleEndian);
}

TEST(Configuration, test_load_config_vibration_template_dualsense)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);

    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x054c, 0x0ce6, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.profile, "dualsense5");
    EXPECT_EQ(config.rumble.packetSize, 63);
    EXPECT_EQ(config.rumble.packet[0], 0x02);
    EXPECT_EQ(config.rumble.packet[1], 0x03);
    EXPECT_EQ(config.rumble.high.offset, 3);
    EXPECT_EQ(config.rumble.low.offset, 4);
}

TEST(Configuration, test_load_config_vibration_template_gamecube_adapter)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);

    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x0079, 0x1846, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.rumble.packetSize, 2);
    EXPECT_EQ(config.rumble.high.offset, 0);
    EXPECT_EQ(config.rumble.low.offset, 1);
}

TEST(Configuration, test_load_config_motorless_stick_on_the_dualshock4_profile)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);

    // Qanba Obsidian: an arcade stick that shares the dualshock4 profile, so the profile must
    // not be the thing that carries the rumble report.
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x2c22, 0x2300, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.profile, "dualshock4");
    EXPECT_FALSE(config.rumble.IsValid());
}

TEST(Configuration, test_load_config_without_vibration_template)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);

    // A pad with no motors must not claim rumble support.
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x0583, 0x2060, false, "", controllerlib::InputDeviceKind::Gamepad);
    EXPECT_EQ(rc, 0);

    EXPECT_FALSE(config.rumble.IsValid());
}

TEST(Configuration, test_load_config_keyboard_uses_its_own_baseline)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x046d, 0xc31c, false, "keyboard", controllerlib::InputDeviceKind::Keyboard);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "keyboard");

    // [default] is gamepad-shaped and must not reach a keyboard.
    EXPECT_EQ(config.analogDeadzonePercent[AnalogAxis::X], 0);
    EXPECT_EQ(config.buttonsPin[GamepadButton::DPAD_UP][0], 0);
}

TEST(Configuration, test_load_config_mouse_reads_its_sensitivity)
{
    ControllerConfig config;

    syscon::StdFileManager fileManager;
    ::syscon::config::Initialize(fileManager);
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x046d, 0xc077, false, "mouse", controllerlib::InputDeviceKind::Mouse);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "mouse");
    EXPECT_EQ(config.mouseSensitivityPercent, 100);
    EXPECT_EQ(config.analogDeadzonePercent[AnalogAxis::X], 0);
}
