#include "switch.h"
#include "config_handler.h"
#include "Controllers.h"
#include "ControllerConfig.h"
#include "logger.h"
#include <cstring>
#include <cstdlib>
#include <stratosphere.hpp>
#include <stratosphere/util/util_ini.hpp>

namespace syscon::config
{
    namespace
    {
        // Utils function
        std::string convertToLowercase(const std::string &str)
        {
            std::string result = "";
            for (char ch : str)
                result += tolower(ch);
            return result;
        }

        class ConfigINIData
        {
        public:
            std::string ini_section;
            ControllerConfig *controller_config;
            GlobalConfig *global_config;
            bool ini_section_found;

            ConfigINIData(const std::string &ini_section_str, ControllerConfig *config) : ini_section(convertToLowercase(ini_section_str)),
                                                                                          controller_config(config),
                                                                                          global_config(NULL),
                                                                                          ini_section_found(false)
            {
            }

            ConfigINIData(const std::string &ini_section_str, GlobalConfig *config) : ini_section(convertToLowercase(ini_section_str)),
                                                                                      controller_config(NULL),
                                                                                      global_config(config),
                                                                                      ini_section_found(false)
            {
            }
        };

        ControllerButton stringToButton(const char *name)
        {
            std::string nameStr = convertToLowercase(name);

            if (nameStr == "b")
                return ControllerButton::B;
            else if (nameStr == "a")
                return ControllerButton::A;
            else if (nameStr == "x")
                return ControllerButton::X;
            else if (nameStr == "y")
                return ControllerButton::Y;
            else if (nameStr == "lstick_click")
                return ControllerButton::LSTICK_CLICK;
            else if (nameStr == "lstick_left")
                return ControllerButton::LSTICK_LEFT;
            else if (nameStr == "lstick_right")
                return ControllerButton::LSTICK_RIGHT;
            else if (nameStr == "lstick_up")
                return ControllerButton::LSTICK_UP;
            else if (nameStr == "lstick_down")
                return ControllerButton::LSTICK_DOWN;
            else if (nameStr == "rstick_click")
                return ControllerButton::RSTICK_CLICK;
            else if (nameStr == "rstick_left")
                return ControllerButton::RSTICK_LEFT;
            else if (nameStr == "rstick_right")
                return ControllerButton::RSTICK_RIGHT;
            else if (nameStr == "rstick_up")
                return ControllerButton::RSTICK_UP;
            else if (nameStr == "rstick_down")
                return ControllerButton::RSTICK_DOWN;
            else if (nameStr == "l")
                return ControllerButton::L;
            else if (nameStr == "r")
                return ControllerButton::R;
            else if (nameStr == "zl")
                return ControllerButton::ZL;
            else if (nameStr == "zr")
                return ControllerButton::ZR;
            else if (nameStr == "minus")
                return ControllerButton::MINUS;
            else if (nameStr == "plus")
                return ControllerButton::PLUS;
            else if (nameStr == "dpad_up")
                return ControllerButton::DPAD_UP;
            else if (nameStr == "dpad_right")
                return ControllerButton::DPAD_RIGHT;
            else if (nameStr == "dpad_down")
                return ControllerButton::DPAD_DOWN;
            else if (nameStr == "dpad_left")
                return ControllerButton::DPAD_LEFT;
            else if (nameStr == "capture")
                return ControllerButton::CAPTURE;
            else if (nameStr == "home")
                return ControllerButton::HOME;

            return ControllerButton::NONE;
        }

        RGBAColor hexStringColorToRGBA(const char *value)
        {
            RGBAColor color;
            color.rgbaValue = 0x000000FF;

            if (value[0] == '#')
                value = &value[1];

            if (strlen(value) == 8) // R G B A
            {
                color.rgbaValue = strtol(value, NULL, 16);
            }
            else if (strlen(value) == 6) // R G B
            {
                color.rgbaValue = strtol(value, NULL, 16);
                color.rgbaValue = (color.rgbaValue << 8) | 0xFF; // Add Alpha
            }
            else
            {
                syscon::logger::LogError("Invalid color value: %s (Invalid length (Expecting 8 or 6 got %d))", value, strlen(value));
            }

            return color;
        }

        bool stringToAnalogConfig(const std::string &cfg, ControllerAnalogConfig *analogCfg)
        {
            std::string stickcfg = convertToLowercase(cfg);

            analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_Unknown;
            analogCfg->sign = stickcfg[0] == '-' ? -1.0 : 1.0;

            if (stickcfg[0] == '-' || stickcfg[0] == '+')
                stickcfg = stickcfg.substr(1);

            if (stickcfg == "x")
                analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_X;
            else if (stickcfg == "y")
                analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_Y;
            else if (stickcfg == "z")
                analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_Z;
            else if (stickcfg == "rz")
                analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_Rz;
            else if (stickcfg == "rx")
                analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_Rx;
            else if (stickcfg == "ry")
                analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_Ry;
            else if (stickcfg == "slider")
                analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_Slider;
            else if (stickcfg == "dial")
                analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_Dial;
            else if (stickcfg == "none")
                analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_Unknown;
            else
                return false;

            return true;
        }

        void parseHotKey(const char *value, ControllerButton hotkeys[2])
        {
            char *tok = strtok(const_cast<char *>(value), "+");

            for (int i = 0; i < 2; i++)
            {
                if (tok == NULL)
                    break;

                hotkeys[i] = stringToButton(tok);
                tok = strtok(NULL, "+");
            }
        }

        bool is_number(const std::string &s)
        {
            std::string::const_iterator it = s.begin();
            while (it != s.end() && std::isdigit(*it))
                ++it;
            return !s.empty() && it == s.end();
        }

        void parseBinding(const char *value, uint8_t button_pin[MAX_PIN_BY_BUTTONS], ControllerAnalogConfig *analogCfg)
        {
            int button_pin_idx = 0;
            char *tok = strtok(const_cast<char *>(value), ",");

            // Reset binding when a new one is found
            analogCfg->bind = ControllerAnalogBinding::ControllerAnalogBinding_Unknown;
            analogCfg->sign = 0.0;
            for (int i = 0; i < MAX_PIN_BY_BUTTONS; i++)
                button_pin[i] = 0;

            while (tok != NULL)
            {
                if (is_number(tok))
                {
                    int pin = atoi(tok);

                    if (button_pin_idx < MAX_PIN_BY_BUTTONS)
                    {
                        if (pin < MAX_CONTROLLER_BUTTONS)
                            button_pin[button_pin_idx++] = pin;
                        else
                            syscon::logger::LogError("Invalid PIN: %d (Max: %d) - Ignoring it !", pin, MAX_CONTROLLER_BUTTONS);
                    }
                    else
                        syscon::logger::LogError("Too many button pin configured (Max: %d) (Ignoring pin: %d)", MAX_PIN_BY_BUTTONS, pin);
                }
                else
                {
                    if (!stringToAnalogConfig(tok, analogCfg))
                        syscon::logger::LogError("Invalid button/axis: %s (Ignoring it) !", tok);
                }

                tok = strtok(NULL, ",");
            }
        }

        ControllerType stringToControllerType(const char *value)
        {
            std::string type = convertToLowercase(value);

            if (type == "prowithbattery")
                return ControllerType_ProWithBattery;
            else if (type == "tarragon")
                return ControllerType_Tarragon;
            else if (type == "snes")
                return ControllerType_Snes;
            else if (type == "pokeballplus")
                return ControllerType_PokeballPlus;
            else if (type == "gamecube")
                return ControllerType_Gamecube;
            else if (type == "pro")
                return ControllerType_Pro;
            else if (type == "3rdpartypro")
                return ControllerType_3rdPartyPro;
            else if (type == "n64")
                return ControllerType_N64;
            else if (type == "sega")
                return ControllerType_Sega;
            else if (type == "nes")
                return ControllerType_Nes;
            else if (type == "famicom")
                return ControllerType_Famicom;

            return ControllerType_Unknown;
        }

        int ParseGlobalConfigLine(void *data, const char *section, const char *name, const char *value)
        {
            ConfigINIData *ini_data = static_cast<ConfigINIData *>(data);
            std::string sectionStr = convertToLowercase(section);
            std::string nameStr = convertToLowercase(name);

            if (ini_data->ini_section != sectionStr)
                return 1; // Not the section we are looking for (return success to continue parsing)

            ini_data->ini_section_found = true;

            if (nameStr == "polling_frequency_ms")
                ini_data->global_config->polling_frequency_ms = atoi(value);
            else if (nameStr == "polling_thread_priority")
                ini_data->global_config->polling_thread_priority = atoi(value);
            else if (nameStr == "log_level")
                ini_data->global_config->log_level = atoi(value);
            else if (nameStr == "discovery_mode")
                ini_data->global_config->discovery_mode = static_cast<DiscoveryMode>(atoi(value));
            else if (nameStr == "auto_add_controller")
                ini_data->global_config->auto_add_controller = (atoi(value) == 0) ? false : true;
            else if (nameStr == "discovery_vidpid")
            {
                char *tok = strtok(const_cast<char *>(value), ",");

                while (tok != NULL)
                {
                    ini_data->global_config->discovery_vidpid.push_back(ControllerVidPid(tok));
                    tok = strtok(NULL, ",");
                }
            }
            else
            {
                syscon::logger::LogError("Unknown key: %s, continue anyway ...", name);
            }

            return 1; // Success
        }

        int ParseControllerConfigLine(void *data, const char *section, const char *name, const char *value)
        {
            ConfigINIData *ini_data = static_cast<ConfigINIData *>(data);
            std::string sectionStr = convertToLowercase(section);
            std::string nameStr = convertToLowercase(name);

            // syscon::logger::LogTrace("Parsing controller config line: %s, %s, %s (expect: %s)", section, name, value, ini_data->ini_section.c_str());
            if (ini_data->ini_section != sectionStr)
                return 1; // Not the section we are looking for (return success to continue parsing)

            ini_data->ini_section_found = true;

            ControllerButton buttonId = stringToButton(nameStr.c_str());
            if (buttonId != ControllerButton::NONE)
                parseBinding(value, ini_data->controller_config->buttonsPin[buttonId], &ini_data->controller_config->buttonsAnalog[buttonId]);
            else if (nameStr == "driver")
                ini_data->controller_config->driver = convertToLowercase(value);
            else if (nameStr == "profile")
                ini_data->controller_config->profile = convertToLowercase(value);
            else if (nameStr == "input_max_packet_size")
                ini_data->controller_config->inputMaxPacketSize = atoi(value);
            else if (nameStr == "output_max_packet_size")
                ini_data->controller_config->outputMaxPacketSize = atoi(value);
            else if (nameStr == "controller_type")
                ini_data->controller_config->controllerType = stringToControllerType(value);
            else if (nameStr == "simulate_home")
                parseHotKey(value, ini_data->controller_config->simulateHome);
            else if (nameStr == "simulate_capture")
                parseHotKey(value, ini_data->controller_config->simulateCapture);
            else if (nameStr == "deadzone_x")
                ini_data->controller_config->analogDeadzonePercent[ControllerAnalogBinding::ControllerAnalogBinding_X] = atoi(value);
            else if (nameStr == "deadzone_y")
                ini_data->controller_config->analogDeadzonePercent[ControllerAnalogBinding::ControllerAnalogBinding_Y] = atoi(value);
            else if (nameStr == "deadzone_z")
                ini_data->controller_config->analogDeadzonePercent[ControllerAnalogBinding::ControllerAnalogBinding_Z] = atoi(value);
            else if (nameStr == "deadzone_rz")
                ini_data->controller_config->analogDeadzonePercent[ControllerAnalogBinding::ControllerAnalogBinding_Rz] = atoi(value);
            else if (nameStr == "deadzone_rx")
                ini_data->controller_config->analogDeadzonePercent[ControllerAnalogBinding::ControllerAnalogBinding_Rx] = atoi(value);
            else if (nameStr == "deadzone_ry")
                ini_data->controller_config->analogDeadzonePercent[ControllerAnalogBinding::ControllerAnalogBinding_Ry] = atoi(value);
            else if (nameStr == "deadzone_slider")
                ini_data->controller_config->analogDeadzonePercent[ControllerAnalogBinding::ControllerAnalogBinding_Slider] = atoi(value);
            else if (nameStr == "deadzone_dial")
                ini_data->controller_config->analogDeadzonePercent[ControllerAnalogBinding::ControllerAnalogBinding_Dial] = atoi(value);
            else if (nameStr == "factor_x")
                ini_data->controller_config->analogFactorPercent[ControllerAnalogBinding::ControllerAnalogBinding_X] = atoi(value);
            else if (nameStr == "factor_y")
                ini_data->controller_config->analogFactorPercent[ControllerAnalogBinding::ControllerAnalogBinding_Y] = atoi(value);
            else if (nameStr == "factor_z")
                ini_data->controller_config->analogFactorPercent[ControllerAnalogBinding::ControllerAnalogBinding_Z] = atoi(value);
            else if (nameStr == "factor_rz")
                ini_data->controller_config->analogFactorPercent[ControllerAnalogBinding::ControllerAnalogBinding_Rz] = atoi(value);
            else if (nameStr == "factor_rx")
                ini_data->controller_config->analogFactorPercent[ControllerAnalogBinding::ControllerAnalogBinding_Rx] = atoi(value);
            else if (nameStr == "factor_ry")
                ini_data->controller_config->analogFactorPercent[ControllerAnalogBinding::ControllerAnalogBinding_Ry] = atoi(value);
            else if (nameStr == "factor_slider")
                ini_data->controller_config->analogFactorPercent[ControllerAnalogBinding::ControllerAnalogBinding_Slider] = atoi(value);
            else if (nameStr == "factor_dial")
                ini_data->controller_config->analogFactorPercent[ControllerAnalogBinding::ControllerAnalogBinding_Dial] = atoi(value);
            else if (nameStr == "color_body")
                ini_data->controller_config->bodyColor = hexStringColorToRGBA(value);
            else if (nameStr == "color_buttons")
                ini_data->controller_config->buttonsColor = hexStringColorToRGBA(value);
            else if (nameStr == "color_leftgrip")
                ini_data->controller_config->leftGripColor = hexStringColorToRGBA(value);
            else if (nameStr == "color_rightgrip")
                ini_data->controller_config->rightGripColor = hexStringColorToRGBA(value);
            else
            {
                syscon::logger::LogError("Unknown key: %s, continue anyway ...", name);
            }
            return 1; // Success
        }

        ams::Result ReadFromConfig(const char *path, ams::util::ini::Handler h, void *config)
        {
            ams::fs::FileHandle file;

            if (R_FAILED(ams::fs::OpenFile(std::addressof(file), path, ams::fs::OpenMode_Read)))
            {
                syscon::logger::LogError("Unable to open configuration file: '%s' !", path);
                return 1;
            }
            ON_SCOPE_EXIT { ams::fs::CloseFile(file); };

            /* Parse the config. */
            Result rc = ams::util::ini::ParseFile(file, config, h);
            if (R_FAILED(rc))
            {
                syscon::logger::LogError("Failed to parse configuration file: '%s' !", path);
                return 1;
            }

            R_SUCCEED();
        }
    } // namespace

    ams::Result LoadGlobalConfig(GlobalConfig *config)
    {
        ConfigINIData cfg("global", config);

        syscon::logger::LogDebug("Loading global config: '%s' ...", CONFIG_FULLPATH);

        R_TRY(ReadFromConfig(CONFIG_FULLPATH, ParseGlobalConfigLine, &cfg));

        R_SUCCEED();
    }

    ams::Result AddControllerToConfig(const char *path, std::string section, std::string profile)
    {
        s64 fileOffset = 0;
        std::stringstream ss;
        ams::fs::FileHandle file;

        /* Get the current time. */
        ams::time::PosixTime time;
        ams::time::StandardUserSystemClock::GetCurrentTime(&time);
        ams::time::CalendarTime calendar = ams::time::impl::util::ToCalendarTimeInUtc(time);

        /* Open the config file. */
        if (R_FAILED(ams::fs::OpenFile(std::addressof(file), path, ams::fs::OpenMode_Write | ams::fs::OpenMode_AllowAppend)))
        {
            syscon::logger::LogError("Unable to open configuration file: '%s' !", path);
            return 1;
        }

        ON_SCOPE_EXIT { ams::fs::CloseFile(file); };

        /* Write the config. */
        ss << std::endl;
        ss << "[" << section << "] ;Automatically added on " << static_cast<int>(calendar.year) << "-" << static_cast<int>(calendar.month) << "-" << static_cast<int>(calendar.day) << " " << static_cast<int>(calendar.hour) << ":" << static_cast<int>(calendar.minute) << ":" << static_cast<int>(calendar.second) << "UTC" << std::endl;
        if (profile != "")
        {
            ss << "profile=" << profile << std::endl;
        }
        else
        {
            ss << "b=1" << std::endl;
            ss << "a=2" << std::endl;
            ss << "x=3" << std::endl;
            ss << "y=4" << std::endl;
            ss << "l=5" << std::endl;
            ss << "r=6" << std::endl;
            ss << "zl=7" << std::endl;
            ss << "zr=8" << std::endl;
            ss << "minus=9" << std::endl;
            ss << "plus=10" << std::endl;
            ss << "capture=11" << std::endl;
            ss << "home=12" << std::endl;
        }

        R_TRY(ams::fs::GetFileSize(&fileOffset, file));

        ams::Result rc = ams::fs::WriteFile(file, fileOffset, ss.str().c_str(), ss.str().length(), ams::fs::WriteOption::Flush);
        if (R_FAILED(rc))
        {
            syscon::logger::LogError("Failed to write configuration file: '%s' !", path);
            return 1;
        }

        R_SUCCEED();
    }

    ams::Result LoadControllerConfig(ControllerConfig *config, uint16_t vendor_id, uint16_t product_id, bool auto_add_controller, const std::string &default_profile)
    {
        ControllerVidPid controllerVidPid(vendor_id, product_id);
        ConfigINIData cfg_default("default", config);
        ConfigINIData cfg_controller(controllerVidPid, config);

        syscon::logger::LogDebug("Loading controller config: '%s' [default] ...", CONFIG_FULLPATH);
        R_TRY(ReadFromConfig(CONFIG_FULLPATH, ParseControllerConfigLine, &cfg_default));

        // Override with vendor specific config
        syscon::logger::LogDebug("Loading controller config: '%s' [%s] ...", CONFIG_FULLPATH, std::string(controllerVidPid).c_str());
        R_TRY(ReadFromConfig(CONFIG_FULLPATH, ParseControllerConfigLine, &cfg_controller));

        if (!cfg_controller.ini_section_found && auto_add_controller)
        {
            syscon::logger::LogDebug("Controller not found in config file, adding it as '%s'...", default_profile.c_str());
            R_TRY(AddControllerToConfig(CONFIG_FULLPATH, std::string(controllerVidPid), default_profile));

            syscon::logger::LogDebug("Reloading controller config: '%s' [%s] ...", CONFIG_FULLPATH, std::string(controllerVidPid).c_str());
            R_TRY(ReadFromConfig(CONFIG_FULLPATH, ParseControllerConfigLine, &cfg_controller));
        }

        // Check if have a "profile"
        if (config->profile.length() > 0)
        {
            syscon::logger::LogDebug("Loading controller config: '%s' (Profile: [%s]) ... ", CONFIG_FULLPATH, config->profile.c_str());
            ConfigINIData cfg_profile(config->profile, config);
            R_TRY(ReadFromConfig(CONFIG_FULLPATH, ParseControllerConfigLine, &cfg_profile));

            // Re-Override with vendor specific config
            // We are doing this to allow the profile to be overrided by the vendor specific config
            // In other words we would like to have [default] overrided by [profile] overrided by [vid-pid]
            R_TRY(ReadFromConfig(CONFIG_FULLPATH, ParseControllerConfigLine, &cfg_controller));
        }

        if (config->buttonsPin[ControllerButton::B][0] == 0 && config->buttonsPin[ControllerButton::A][0] == 0 && config->buttonsPin[ControllerButton::Y][0] == 0 && config->buttonsPin[ControllerButton::X][0] == 0)
            syscon::logger::LogError("No buttons configured for this controller [%04x-%04x] - Stick might works but buttons will not work (https://github.com/o0Zz/sys-con/blob/master/doc/Troubleshooting.md)", vendor_id, product_id);
        else
            syscon::logger::LogInfo("Controller successfully loaded (B=%d, A=%d, Y=%d, X=%d, ...) !", config->buttonsPin[ControllerButton::B][0], config->buttonsPin[ControllerButton::A][0], config->buttonsPin[ControllerButton::Y][0], config->buttonsPin[ControllerButton::X][0]);

        R_SUCCEED();
    }
} // namespace syscon::config