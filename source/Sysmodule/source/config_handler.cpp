#include "config_handler.h"

#include "logger.h"
#include "ini.h"

#include <cstring>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <stratosphere.hpp>

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
            analogCfg->sign = stickcfg[0] == '-' ? -1.0f : 1.0f;

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

            // syscon::logger::LogTrace("Parsing global config line: %s, %s, %s (expect: %s)", section, name, value, ini_data->ini_section.c_str());
            if (ini_data->ini_section != sectionStr)
                return 1; // Not the section we are looking for (return success to continue parsing)

            ini_data->ini_section_found = true;

            if (nameStr == "polling_timeout_ms")
                ini_data->global_config->polling_timeout_ms = atoi(value);
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
            {
                ini_data->controller_config->buttonsAnalogUsed = true;
                parseBinding(value, ini_data->controller_config->buttonsPin[buttonId], &ini_data->controller_config->buttonsAnalog[buttonId]);
            }
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
            else if (nameStr.starts_with("simulate_"))
            {
                ControllerButton btn = stringToButton(nameStr.substr(9).c_str());
                if (btn != ControllerButton::NONE && btn < ControllerButton::COUNT)
                {
                    for (int i = 0; i < MAX_CONTROLLER_COMBO; i++)
                    {
                        if (ini_data->controller_config->simulateCombos[i].buttonSimulated != ControllerButton::NONE)
                            continue; // Search for a free slot

                        ini_data->controller_config->simulateCombos[i].buttonSimulated = btn;
                        parseHotKey(value, ini_data->controller_config->simulateCombos[i].buttons);
                        break; // Found a free slot
                    }
                }
                else
                    syscon::logger::LogError("Unknown key: %s, continue anyway ...", nameStr.c_str());
            }
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

#ifdef ATMOSPHERE_OS_HORIZON

        char ini_buffer[32 * 1024]; // 32 KiB buffer for the INI file
        int ReadFromConfig(const char *path, ini_handler h, void *config)
        {
            ams::fs::FileHandle file;

            Result rc = ams::fs::OpenFile(std::addressof(file), path, ams::fs::OpenMode_Read).GetValue();
            if (R_FAILED(rc))
            {
                syscon::logger::LogError("Unable to open configuration file: '%s' !", path);
                return rc;
            }
            ON_SCOPE_EXIT { ams::fs::CloseFile(file); };

            /* Parse the config. */
            ams::fs::ReadFile(file, 0, ini_buffer, sizeof(ini_buffer));

            return ini_parse_string(ini_buffer, h, config);
        }
#else
        int ReadFromConfig(const char *path, ini_handler h, void *config)
        {
            return ini_parse(path, h, config);
        }
#endif

    } // namespace

    int LoadGlobalConfig(const std::string &configFullPath, GlobalConfig *config)
    {
        ConfigINIData cfg("global", config);

        syscon::logger::LogDebug("Loading global config: '%s' ...", configFullPath.c_str());

        int rc = ReadFromConfig(configFullPath.c_str(), ParseGlobalConfigLine, &cfg);
        if (rc)
        {
            syscon::logger::LogError("Failed to load global config: '%s' (Error: 0x%08X) !", configFullPath.c_str(), rc);
            return rc;
        }

        return 0;
    }

    int AddControllerToConfig(const std::string &path, const std::string &section, const std::string &profile)
    {
        std::stringstream ss;

        // Get the current time.
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        auto timeinfo = *std::localtime(&timeT);

        // Check if the file exists and is accessible.
        if (!std::filesystem::exists(path))
        {
            syscon::logger::LogError("Error: Configuration file does not exist: %s", path.c_str());
            return -1; // Replace with appropriate error code.
        }

        // Open the file for appending.
        std::ofstream configFile(path, std::ios::app);
        if (!configFile.is_open())
        {
            syscon::logger::LogError("Error: Unable to open configuration file: %s", path.c_str());
            return -1; // Replace with appropriate error code.
        }

        // Write the new section and profile data.
        ss << "\n";
        ss << "[" << section << "] ; Automatically added on " << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S UTC") << "\n";

        if (!profile.empty())
        {
            ss << "profile=" << profile << "\n";
        }
        else
        {
            ss << "b=1\n"
               << "a=2\n"
               << "x=3\n"
               << "y=4\n"
               << "l=5\n"
               << "r=6\n"
               << "zl=7\n"
               << "zr=8\n"
               << "minus=9\n"
               << "plus=10\n"
               << "capture=11\n"
               << "home=12\n";
        }

        configFile << ss.str();

        if (configFile.fail())
        {
            syscon::logger::LogError("Error: Failed to write to configuration file: %s", path.c_str());
            return -1;
        }

        return 0;
    }

    int LoadControllerConfig(const std::string &configFullPath, ControllerConfig *config, uint16_t vendor_id, uint16_t product_id, bool auto_add_controller, const std::string &default_profile)
    {
        ControllerVidPid controllerVidPid(vendor_id, product_id);
        ConfigINIData cfg_default("default", config);
        ConfigINIData cfg_controller(controllerVidPid, config);

        syscon::logger::LogDebug("Loading controller config: '%s' [default] ...", configFullPath.c_str());

        int rc = ReadFromConfig(configFullPath.c_str(), ParseControllerConfigLine, &cfg_default);
        if (rc)
            return rc;

        // Override with vendor specific config
        syscon::logger::LogDebug("Loading controller config: '%s' [%s] ...", configFullPath.c_str(), std::string(controllerVidPid).c_str());
        rc = ReadFromConfig(configFullPath.c_str(), ParseControllerConfigLine, &cfg_controller);
        if (rc)
            return rc;

        if (!cfg_controller.ini_section_found && auto_add_controller)
        {
            syscon::logger::LogDebug("Controller not found in config file, adding it as '%s'...", default_profile.c_str());
            rc = AddControllerToConfig(configFullPath.c_str(), std::string(controllerVidPid), default_profile);
            if (rc)
                return rc;

            syscon::logger::LogDebug("Reloading controller config: '%s' [%s] ...", configFullPath.c_str(), std::string(controllerVidPid).c_str());
            rc = ReadFromConfig(configFullPath.c_str(), ParseControllerConfigLine, &cfg_controller);
            if (rc)
                return rc;
        }

        // Check if have a "profile"
        if (config->profile.length() > 0)
        {
            syscon::logger::LogDebug("Loading controller config: '%s' (Profile: [%s]) ... ", configFullPath.c_str(), config->profile.c_str());
            ConfigINIData cfg_profile(config->profile, config);
            rc = ReadFromConfig(configFullPath.c_str(), ParseControllerConfigLine, &cfg_profile);
            if (rc)
                return rc;

            // Re-Override with vendor specific config
            // We are doing this to allow the profile to be overrided by the vendor specific config
            // In other words we would like to have [default] overrided by [profile] overrided by [vid-pid]
            rc = ReadFromConfig(configFullPath.c_str(), ParseControllerConfigLine, &cfg_controller);
            if (rc)
                return rc;
        }

        if (config->buttonsPin[ControllerButton::B][0] == 0 && config->buttonsPin[ControllerButton::A][0] == 0 && config->buttonsPin[ControllerButton::Y][0] == 0 && config->buttonsPin[ControllerButton::X][0] == 0)
            syscon::logger::LogError("No buttons configured for this controller [%04x-%04x] - Stick might works but buttons will not work (https://github.com/o0Zz/sys-con/blob/master/doc/Troubleshooting.md)", vendor_id, product_id);
        else
            syscon::logger::LogInfo("Controller successfully loaded (B=%d, A=%d, Y=%d, X=%d, ...) !", config->buttonsPin[ControllerButton::B][0], config->buttonsPin[ControllerButton::A][0], config->buttonsPin[ControllerButton::Y][0], config->buttonsPin[ControllerButton::X][0]);

        return 0;
    }
} // namespace syscon::config