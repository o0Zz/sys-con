#include <gtest/gtest.h>
#include "drivers/BaseController.h"
#include "config_handler.h"
#include "StdFileManager.h"

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
using namespace controllerlib;


// CONFIG_FULLPATH_PROJECT is supplied by tests/CMakeLists.txt as an absolute path, so the
// tests work from any build directory. The fallback keeps a hand-run build working.
#ifndef CONFIG_FULLPATH_PROJECT
    #define CONFIG_FULLPATH_PROJECT "../../dist/config/sys-con/config.ini"
#endif

TEST(Configuration, test_load_config_unknown)
{
    ControllerConfig config;

    ::syscon::config::Initialize(std::make_unique<syscon::StdFileManager>());
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x0000, 0x0000, false, "");
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

    ::syscon::config::Initialize(std::make_unique<syscon::StdFileManager>());
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x054c, 0x0cda, false, "");
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

    ::syscon::config::Initialize(std::make_unique<syscon::StdFileManager>());
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x045e, 0x02dd, false, "");
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

    ::syscon::config::Initialize(std::make_unique<syscon::StdFileManager>());
    int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH_PROJECT, &config, 0x057e, 0x0337, false, "");
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(config.driver, "wii");
    EXPECT_EQ(config.profile, "wii");
    EXPECT_EQ(config.buttonsPin[GamepadButton::ZL][0], 0);
}