#include <gtest/gtest.h>
#include "Controllers/BaseController.h"
#include "config_handler.h"
#include "filemanager_std.h"

#define CONFIG_FULLPATH_PROJECT "../../dist/config/sys-con/config.ini"

TEST(Configuration, test_load_config_unknown)
{
    ControllerConfig config;

    ::syscon::config::Initialize(std::make_unique<syscon::StdFileManager>());
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x0000, 0x0000, false, "");
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "");
    EXPECT_EQ(config.profile, "");
    EXPECT_EQ(config.controllerType, ControllerType_Pro);
    EXPECT_EQ(config.buttonsPin[ControllerButton::X][0], 0);
    EXPECT_EQ(config.buttonsPin[ControllerButton::A][0], 0);
    EXPECT_EQ(config.buttonsPin[ControllerButton::B][0], 0);
    EXPECT_EQ(config.buttonsPin[ControllerButton::Y][0], 0);
}

TEST(Configuration, test_load_config_no_profile)
{
    ControllerConfig config;

    ::syscon::config::Initialize(std::make_unique<syscon::StdFileManager>());
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x054c, 0x0cda, false, "");
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "");
    EXPECT_EQ(config.profile, "");
    EXPECT_EQ(config.controllerType, ControllerType_Pro);
    EXPECT_EQ(config.buttonsPin[ControllerButton::X][0], 1);
    EXPECT_EQ(config.buttonsPin[ControllerButton::A][0], 2);
    EXPECT_EQ(config.buttonsPin[ControllerButton::B][0], 3);
    EXPECT_EQ(config.buttonsPin[ControllerButton::Y][0], 4);
}

TEST(Configuration, test_load_config_with_profile_xboxone)
{
    ControllerConfig config;

    ::syscon::config::Initialize(std::make_unique<syscon::StdFileManager>());
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x045e, 0x02dd, false, "");
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "xboxone");
    EXPECT_EQ(config.profile, "xboxone");
    EXPECT_EQ(config.controllerType, ControllerType_Pro);
    EXPECT_EQ(config.buttonsPin[ControllerButton::X][0], 4);
    EXPECT_EQ(config.buttonsPin[ControllerButton::A][0], 2);
    EXPECT_EQ(config.buttonsPin[ControllerButton::B][0], 1);
    EXPECT_EQ(config.buttonsPin[ControllerButton::Y][0], 3);
    EXPECT_EQ(config.simulateCombos[0].buttonSimulated, ControllerButton::CAPTURE);
    EXPECT_EQ(config.simulateCombos[1].buttonSimulated, ControllerButton::HOME);
    EXPECT_EQ(config.simulateCombos[2].buttonSimulated, ControllerButton::NONE);
}

TEST(Configuration, test_load_config_adaptoid_n64)
{
    ControllerConfig config;

    ::syscon::config::Initialize(std::make_unique<syscon::StdFileManager>());
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x06f7, 0x0001, false, "");
    EXPECT_EQ(rc, 0);

    // controller_type=n64 is deliberately not set: the Switch's N64 core doesn't reliably read
    // C-buttons from a third-party HDLS "Lagon" pad, so this falls back to the [default] pro type
    // and drives C-buttons through the right stick instead (see config.ini for details).
    EXPECT_EQ(config.controllerType, ControllerType_Pro);
    EXPECT_EQ(config.buttonsPin[ControllerButton::A][0], 1);
    EXPECT_EQ(config.buttonsPin[ControllerButton::B][0], 4);
    EXPECT_EQ(config.buttonsPin[ControllerButton::L][0], 7);
    EXPECT_EQ(config.buttonsPin[ControllerButton::R][0], 8);
    EXPECT_EQ(config.buttonsPin[ControllerButton::PLUS][0], 9);
    EXPECT_EQ(config.buttonsPin[ControllerButton::ZL][0], 10);
    EXPECT_EQ(config.buttonsPin[ControllerButton::DPAD_UP][0], 11);
    EXPECT_EQ(config.buttonsPin[ControllerButton::DPAD_DOWN][0], 12);
    EXPECT_EQ(config.buttonsPin[ControllerButton::DPAD_LEFT][0], 13);
    EXPECT_EQ(config.buttonsPin[ControllerButton::DPAD_RIGHT][0], 14);
    EXPECT_EQ(config.buttonsPin[ControllerButton::RSTICK_UP][0], 2);
    EXPECT_EQ(config.buttonsPin[ControllerButton::RSTICK_DOWN][0], 6);
    EXPECT_EQ(config.buttonsPin[ControllerButton::RSTICK_LEFT][0], 5);
    EXPECT_EQ(config.buttonsPin[ControllerButton::RSTICK_RIGHT][0], 3);

    // simulate_capture/simulate_home come from [default] (minus+dpad_up/down). This pad has no
    // Adaptoid-specific combos of its own (tried L+dpad chords for minus/home/zr, but removed them -
    // personal taste, and they collided with in-game L+D-pad use).
    EXPECT_EQ(config.simulateCombos[0].buttonSimulated, ControllerButton::CAPTURE);
    EXPECT_EQ(config.simulateCombos[1].buttonSimulated, ControllerButton::HOME);
    EXPECT_EQ(config.simulateCombos[2].buttonSimulated, ControllerButton::NONE);
}

TEST(Configuration, test_load_config_with_profile_wii)
{
    ControllerConfig config;

    ::syscon::config::Initialize(std::make_unique<syscon::StdFileManager>());
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x057e, 0x0337, false, "");
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "wii");
    EXPECT_EQ(config.profile, "wii");
    EXPECT_EQ(config.buttonsPin[ControllerButton::ZL][0], 0);
}