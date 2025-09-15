#include <gtest/gtest.h>
#include "Controllers/BaseController.h"
#include "config_handler.h"
#include "filemanager_std.h"

#define CONFIG_FULLPATH_PROJECT "../../../dist/config/sys-con/config.ini"

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
