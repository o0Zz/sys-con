/*
    The rules LoadControllerConfig and LoadGlobalConfig apply on top of plain parsing:
    how simulate_ combos combine across layers, what a syntax error costs, and which
    values are refused instead of being wrapped into their field.
*/
#include <gtest/gtest.h>

#include "config_handler.h"
#include "mocks/MemoryFileManager.h"

#include <string>

using namespace controllerlib;

namespace
{
    constexpr const char *kConfigPath = "/config.ini";
    constexpr uint16_t kVendor = 0x1234;
    constexpr uint16_t kProduct = 0x5678;

    struct Loaded
    {
        int rc;
        ControllerConfig config;
    };

    Loaded LoadController(const std::string &contents)
    {
        MemoryFileManager fileManager(contents);
        ::syscon::config::Initialize(fileManager);

        Loaded loaded{};
        loaded.rc = ::syscon::config::LoadControllerConfig(kConfigPath, &loaded.config, kVendor, kProduct, false, "");
        return loaded;
    }

    syscon::config::GlobalConfig LoadGlobal(const std::string &contents)
    {
        MemoryFileManager fileManager(contents);
        ::syscon::config::Initialize(fileManager);

        syscon::config::GlobalConfig config;
        ::syscon::config::LoadGlobalConfig(kConfigPath, &config);
        return config;
    }

    int ComboCount(const ControllerConfig &config)
    {
        int count = 0;
        while (count < MAX_CONTROLLER_COMBO && config.simulateCombos[count].buttonSimulated != GamepadButton::NONE)
            count++;
        return count;
    }
} // namespace

TEST(ConfigRules, test_single_button_combo)
{
    Loaded loaded = LoadController("[default]\nsimulate_home=minus\n");

    ASSERT_EQ(loaded.rc, 0);
    ASSERT_EQ(ComboCount(loaded.config), 1);
    EXPECT_EQ(loaded.config.simulateCombos[0].buttonSimulated, GamepadButton::HOME);
    EXPECT_EQ(loaded.config.simulateCombos[0].buttons[0], GamepadButton::MINUS);
    EXPECT_EQ(loaded.config.simulateCombos[0].buttons[1], GamepadButton::NONE);
}

TEST(ConfigRules, test_vidpid_combo_replaces_default_one)
{
    Loaded loaded = LoadController(
        "[default]\nsimulate_home=minus+dpad_down\nsimulate_capture=minus+dpad_up\n"
        "[1234-5678]\nsimulate_home=plus\n");

    ASSERT_EQ(ComboCount(loaded.config), 2);
    EXPECT_EQ(loaded.config.simulateCombos[0].buttonSimulated, GamepadButton::HOME);
    EXPECT_EQ(loaded.config.simulateCombos[0].buttons[0], GamepadButton::PLUS);
    EXPECT_EQ(loaded.config.simulateCombos[0].buttons[1], GamepadButton::NONE);
    EXPECT_EQ(loaded.config.simulateCombos[1].buttonSimulated, GamepadButton::CAPTURE);
}

TEST(ConfigRules, test_empty_combo_removes_it_and_keeps_the_rest_packed)
{
    Loaded loaded = LoadController(
        "[default]\nsimulate_home=minus+dpad_down\nsimulate_capture=minus+dpad_up\n"
        "[1234-5678]\nsimulate_home=\n");

    ASSERT_EQ(ComboCount(loaded.config), 1);
    EXPECT_EQ(loaded.config.simulateCombos[0].buttonSimulated, GamepadButton::CAPTURE);
}

TEST(ConfigRules, test_vidpid_combo_is_not_added_twice_with_a_profile)
{
    // [vid-pid] is parsed twice when it names a profile, so it can override the profile.
    Loaded loaded = LoadController(
        "[myprofile]\nB=1\n"
        "[1234-5678]\nprofile=myprofile\nsimulate_home=plus+minus\n");

    EXPECT_EQ(ComboCount(loaded.config), 1);
}

TEST(ConfigRules, test_invalid_combo_is_ignored)
{
    Loaded loaded = LoadController("[default]\nsimulate_home=minus+nosuchbutton\n");

    EXPECT_EQ(ComboCount(loaded.config), 0);
}

TEST(ConfigRules, test_syntax_error_does_not_drop_the_vidpid_section)
{
    Loaded loaded = LoadController(
        "[default]\nB=7\nthis line has no equals sign\n"
        "[1234-5678]\nA=3\n");

    EXPECT_EQ(loaded.rc, 0);
    EXPECT_EQ(loaded.config.buttonsPin[GamepadButton::B][0], 7);
    EXPECT_EQ(loaded.config.buttonsPin[GamepadButton::A][0], 3);
}

TEST(ConfigRules, test_missing_profile_still_applies_the_vidpid_section)
{
    Loaded loaded = LoadController("[1234-5678]\nprofile=doesnotexist\nA=3\n");

    EXPECT_EQ(loaded.rc, 0);
    EXPECT_EQ(loaded.config.buttonsPin[GamepadButton::A][0], 3);
}

TEST(ConfigRules, test_out_of_range_analog_values_are_refused)
{
    Loaded loaded = LoadController(
        "[default]\ndeadzone_x=100\ndeadzone_y=25\nfactor_x=300\nfactor_y=150\n");

    EXPECT_EQ(loaded.config.analogDeadzonePercent[AnalogAxis::X], 0);
    EXPECT_EQ(loaded.config.analogDeadzonePercent[AnalogAxis::Y], 25);
    EXPECT_EQ(loaded.config.analogFactorPercent[AnalogAxis::X], 100);
    EXPECT_EQ(loaded.config.analogFactorPercent[AnalogAxis::Y], 150);
}

TEST(ConfigRules, test_out_of_range_global_values_keep_their_default)
{
    syscon::config::GlobalConfig config = LoadGlobal(
        "[global]\npolling_timeout_ms=0\npolling_thread_priority=99\ndiscovery_mode=5\nnetwork_controller_port=70000\n");

    const syscon::config::GlobalConfig defaults;
    EXPECT_EQ(config.polling_timeout_ms, defaults.polling_timeout_ms);
    EXPECT_EQ(config.polling_thread_priority, defaults.polling_thread_priority);
    EXPECT_EQ(config.discovery_mode, defaults.discovery_mode);
    EXPECT_EQ(config.network_controller_port, defaults.network_controller_port);
}

TEST(ConfigRules, test_in_range_global_values_are_taken)
{
    syscon::config::GlobalConfig config = LoadGlobal(
        "[global]\npolling_timeout_ms=4\npolling_thread_priority=30\ndiscovery_mode=2\n");

    EXPECT_EQ(config.polling_timeout_ms, 4);
    EXPECT_EQ(config.polling_thread_priority, 30);
    EXPECT_EQ(config.discovery_mode, syscon::config::DiscoveryMode::VIDPID);
}

TEST(ConfigRules, test_discovery_vidpid_without_a_dash_is_skipped)
{
    syscon::config::GlobalConfig config = LoadGlobal("[global]\ndiscovery_vidpid=054c,0f0d-0088,054c-*\n");

    ASSERT_EQ(config.discovery_vidpid.size(), 2u);
    EXPECT_EQ(config.discovery_vidpid[0].vid, 0x0f0d);
    EXPECT_EQ(config.discovery_vidpid[0].pid, 0x0088);
    EXPECT_EQ(config.discovery_vidpid[1].vid, 0x054c);
    EXPECT_EQ(config.discovery_vidpid[1].pid, 0x0000);
}
