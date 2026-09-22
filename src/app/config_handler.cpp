#include "config_handler.h"

#include "logger.h"
#include "ini.h"

#include <array>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <chrono>

using namespace controllerlib;

// _WIN32, not WIN32: MSVC always defines the former, while the latter only appears if
// windows.h or the build system happens to define it. Under MSBuild it did; under Ninja
// or a bare compiler invocation it does not, and the file then failed to link.
#ifdef _WIN32
    #define strtok_r                       strtok_s
    #define localtime_r(localtime, result) localtime_s(result, localtime)
#endif

namespace syscon::config
{
    namespace
    {
        IFileManager *file_manager = nullptr;

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

        constexpr struct
        {
            std::string_view name;
            GamepadButton button;
        } ButtonNames[] = {
            {"b", GamepadButton::B},
            {"a", GamepadButton::A},
            {"x", GamepadButton::X},
            {"y", GamepadButton::Y},
            {"lstick_click", GamepadButton::LSTICK_CLICK},
            {"lstick_left", GamepadButton::LSTICK_LEFT},
            {"lstick_right", GamepadButton::LSTICK_RIGHT},
            {"lstick_up", GamepadButton::LSTICK_UP},
            {"lstick_down", GamepadButton::LSTICK_DOWN},
            {"rstick_click", GamepadButton::RSTICK_CLICK},
            {"rstick_left", GamepadButton::RSTICK_LEFT},
            {"rstick_right", GamepadButton::RSTICK_RIGHT},
            {"rstick_up", GamepadButton::RSTICK_UP},
            {"rstick_down", GamepadButton::RSTICK_DOWN},
            {"l", GamepadButton::L},
            {"r", GamepadButton::R},
            {"zl", GamepadButton::ZL},
            {"zr", GamepadButton::ZR},
            {"minus", GamepadButton::MINUS},
            {"plus", GamepadButton::PLUS},
            {"dpad_up", GamepadButton::DPAD_UP},
            {"dpad_right", GamepadButton::DPAD_RIGHT},
            {"dpad_down", GamepadButton::DPAD_DOWN},
            {"dpad_left", GamepadButton::DPAD_LEFT},
            {"capture", GamepadButton::CAPTURE},
            {"home", GamepadButton::HOME},
        };

        GamepadButton stringToButton(const char *name)
        {
            std::string nameStr = convertToLowercase(name);

            for (const auto &entry : ButtonNames)
            {
                if (nameStr == entry.name)
                    return entry.button;
            }

            return GamepadButton::NONE;
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

        constexpr struct
        {
            std::string_view name;
            AnalogAxis axis;
        } AxisNames[] = {
            {"x", AnalogAxis::X},
            {"y", AnalogAxis::Y},
            {"z", AnalogAxis::Z},
            {"rz", AnalogAxis::Rz},
            {"rx", AnalogAxis::Rx},
            {"ry", AnalogAxis::Ry},
            {"slider", AnalogAxis::Slider},
            {"dial", AnalogAxis::Dial},
            {"brake", AnalogAxis::Brake},
            {"accelerator", AnalogAxis::Accelerator},
            {"none", AnalogAxis::Unknown},
        };

        bool stringToAnalogConfig(const std::string &cfg, ControllerAnalogConfig *analogCfg)
        {
            std::string stickcfg = convertToLowercase(cfg);

            analogCfg->bind = AnalogAxis::Unknown;
            analogCfg->sign = stickcfg[0] == '-' ? -1.0f : 1.0f;

            if (stickcfg[0] == '-' || stickcfg[0] == '+')
                stickcfg = stickcfg.substr(1);

            for (const auto &entry : AxisNames)
            {
                if (stickcfg == entry.name)
                {
                    analogCfg->bind = entry.axis;
                    return true;
                }
            }

            return false;
        }

        void parseHotKey(const char *value, GamepadButton hotkeys[2])
        {
            char *context;
            char *tok = strtok_r(const_cast<char *>(value), "+", &context);

            for (int i = 0; i < 2; i++)
            {
                if (tok == NULL)
                    break;

                hotkeys[i] = stringToButton(tok);
                tok = strtok_r(NULL, "+", &context);
            }
        }

        bool isNumber(const std::string &s)
        {
            std::string::const_iterator it = s.begin();
            while (it != s.end() && std::isdigit(*it))
                ++it;
            return !s.empty() && it == s.end();
        }

        void parseBinding(const char *value, std::array<PinId, MAX_PIN_BY_BUTTONS> &button_pin, ControllerAnalogConfig *analogCfg)
        {
            int button_pin_idx = 0;
            char *context;
            char *tok = strtok_r(const_cast<char *>(value), ",", &context);

            // Reset binding when a new one is found
            analogCfg->bind = AnalogAxis::Unknown;
            analogCfg->sign = 0.0;
            for (int i = 0; i < MAX_PIN_BY_BUTTONS; i++)
                button_pin[i] = PinId{};

            while (tok != NULL)
            {
                if (isNumber(tok))
                {
                    int pin = atoi(tok);

                    if (button_pin_idx < MAX_PIN_BY_BUTTONS)
                    {
                        if (pin >= 0 && pin < static_cast<int>(MaxPinCount))
                            button_pin[button_pin_idx++] = pin;
                        else
                            syscon::logger::LogError("Invalid PIN: %d (Max: %d) - Ignoring it !", pin, static_cast<int>(MaxPinCount));
                    }
                    else
                        syscon::logger::LogError("Too many button pin configured (Max: %d) (Ignoring pin: %d)", MAX_PIN_BY_BUTTONS, pin);
                }
                else
                {
                    if (!stringToAnalogConfig(tok, analogCfg))
                        syscon::logger::LogError("Invalid button/axis: %s (Ignoring it) !", tok);
                }

                tok = strtok_r(NULL, ",", &context);
            }
        }

        constexpr struct
        {
            std::string_view name;
            ControllerType type;
        } ControllerTypeNames[] = {
            {"prowithbattery", ControllerType_ProWithBattery},
            {"tarragon", ControllerType_Tarragon},
            {"snes", ControllerType_Snes},
            {"pokeballplus", ControllerType_PokeballPlus},
            {"gamecube", ControllerType_Gamecube},
            {"pro", ControllerType_Pro},
            {"3rdpartypro", ControllerType_3rdPartyPro},
            {"n64", ControllerType_N64},
            {"sega", ControllerType_Sega},
            {"nes", ControllerType_Nes},
            {"famicom", ControllerType_Famicom},
        };

        ControllerType stringToControllerType(const char *value)
        {
            std::string type = convertToLowercase(value);

            for (const auto &entry : ControllerTypeNames)
            {
                if (type == entry.name)
                    return entry.type;
            }

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

            if (nameStr == "polling_timeout_ms")
                ini_data->global_config->polling_timeout_ms = atoi(value);
            else if (nameStr == "polling_thread_priority")
                ini_data->global_config->polling_thread_priority = atoi(value);
            else if (nameStr == "log_level")
                ini_data->global_config->log_level = LogLevelFromInt(atoi(value));
            else if (nameStr == "discovery_mode")
                ini_data->global_config->discovery_mode = static_cast<DiscoveryMode>(atoi(value));
            else if (nameStr == "auto_add_controller")
                ini_data->global_config->auto_add_controller = (atoi(value) == 0) ? false : true;
            else if (nameStr == "network_controller")
                ini_data->global_config->network_controller = (atoi(value) == 0) ? false : true;
            else if (nameStr == "network_controller_port")
                ini_data->global_config->network_controller_port = static_cast<uint16_t>(atoi(value));
            else if (nameStr == "mode")
            {
                std::string modeStr = convertToLowercase(value);
                if (modeStr == "mitm")
                    ini_data->global_config->mode = VirtualPadMode::MITM;
                else if (modeStr == "hiddbg")
                    ini_data->global_config->mode = VirtualPadMode::HIDDBG;
                else
                    syscon::logger::LogError("Unknown mode: %s (expected 'mitm' or 'hiddbg') - Ignoring it !", value);
            }
            else if (nameStr == "discovery_vidpid")
            {
                char *context;
                char *tok = strtok_r(const_cast<char *>(value), ",", &context);

                while (tok != NULL)
                {
                    ini_data->global_config->discovery_vidpid.push_back(ControllerVidPid(tok));
                    tok = strtok_r(NULL, ",", &context);
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

            if (ini_data->ini_section != sectionStr)
                return 1; // Not the section we are looking for (return success to continue parsing)

            ini_data->ini_section_found = true;

            GamepadButton buttonId = stringToButton(nameStr.c_str());
            if (buttonId != GamepadButton::NONE)
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
                GamepadButton btn = stringToButton(nameStr.substr(9).c_str());
                if (btn != GamepadButton::NONE && btn < GamepadButton::COUNT)
                {
                    for (int i = 0; i < MAX_CONTROLLER_COMBO; i++)
                    {
                        if (ini_data->controller_config->simulateCombos[i].buttonSimulated != GamepadButton::NONE)
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
                ini_data->controller_config->analogDeadzonePercent[AnalogAxis::X] = atoi(value);
            else if (nameStr == "deadzone_y")
                ini_data->controller_config->analogDeadzonePercent[AnalogAxis::Y] = atoi(value);
            else if (nameStr == "deadzone_z")
                ini_data->controller_config->analogDeadzonePercent[AnalogAxis::Z] = atoi(value);
            else if (nameStr == "deadzone_rz")
                ini_data->controller_config->analogDeadzonePercent[AnalogAxis::Rz] = atoi(value);
            else if (nameStr == "deadzone_rx")
                ini_data->controller_config->analogDeadzonePercent[AnalogAxis::Rx] = atoi(value);
            else if (nameStr == "deadzone_ry")
                ini_data->controller_config->analogDeadzonePercent[AnalogAxis::Ry] = atoi(value);
            else if (nameStr == "deadzone_slider")
                ini_data->controller_config->analogDeadzonePercent[AnalogAxis::Slider] = atoi(value);
            else if (nameStr == "deadzone_dial")
                ini_data->controller_config->analogDeadzonePercent[AnalogAxis::Dial] = atoi(value);
            else if (nameStr == "factor_x")
                ini_data->controller_config->analogFactorPercent[AnalogAxis::X] = atoi(value);
            else if (nameStr == "factor_y")
                ini_data->controller_config->analogFactorPercent[AnalogAxis::Y] = atoi(value);
            else if (nameStr == "factor_z")
                ini_data->controller_config->analogFactorPercent[AnalogAxis::Z] = atoi(value);
            else if (nameStr == "factor_rz")
                ini_data->controller_config->analogFactorPercent[AnalogAxis::Rz] = atoi(value);
            else if (nameStr == "factor_rx")
                ini_data->controller_config->analogFactorPercent[AnalogAxis::Rx] = atoi(value);
            else if (nameStr == "factor_ry")
                ini_data->controller_config->analogFactorPercent[AnalogAxis::Ry] = atoi(value);
            else if (nameStr == "factor_slider")
                ini_data->controller_config->analogFactorPercent[AnalogAxis::Slider] = atoi(value);
            else if (nameStr == "factor_dial")
                ini_data->controller_config->analogFactorPercent[AnalogAxis::Dial] = atoi(value);
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

        // The whole file is parsed from memory: LoadControllerConfig re-parses it once per
        // override layer, and inih's own reader handles the line edge cases.
        bool ReadConfigFile(const std::string &path, std::string *out)
        {
            std::unique_ptr<IFile> file = file_manager->open(path, OpenFlags_Read);
            if (!file)
            {
                syscon::logger::LogError("Unable to open configuration file: '%s' !", path.c_str());
                return false;
            }

            out->resize(file_manager->file_size(path));
            out->resize(file->read(out->data(), out->size()));
            return true;
        }

    } // namespace

    void Initialize(IFileManager &fileManager)
    {
        file_manager = &fileManager;
    }

    int LoadGlobalConfig(const std::string &configFullPath, GlobalConfig *config)
    {
        syscon::logger::LogDebug("Loading global config: '%s' ...", configFullPath.c_str());

        std::string contents;
        if (!ReadConfigFile(configFullPath, &contents))
            return -1;

        ConfigINIData cfg("global", config);
        int rc = ini_parse_string(contents.c_str(), ParseGlobalConfigLine, &cfg);
        if (rc)
            syscon::logger::LogError("Failed to load global config: '%s' (Error: 0x%08X) !", configFullPath.c_str(), rc);

        return rc;
    }

    int AddControllerToConfig(const std::string &path, const std::string &section, const std::string &profile)
    {
        std::stringstream ss;

        // Get the current time.
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        struct tm timeinfo;
        localtime_r(&timeT, &timeinfo);

        if (file_manager == nullptr)
        {
            syscon::logger::LogError("Error: Configuration is not initialized !");
            return -1;
        }

        // Refuse to conjure a config file out of nothing; auto-add only extends an existing one.
        if (file_manager->file_size(path) == 0)
        {
            syscon::logger::LogError("Error: Configuration file does not exist: %s", path.c_str());
            return -1; // Replace with appropriate error code.
        }

        // Open the file for appending.
        std::unique_ptr<IFile> configFile = file_manager->open(path, (OpenFlags)(OpenFlags_Write | OpenFlags_Append));
        if (!configFile || !configFile->is_open())
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

        const std::string payload = ss.str();
        if (configFile->write(payload.data(), payload.size()) != payload.size())
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

        std::string contents;
        if (!ReadConfigFile(configFullPath, &contents))
            return -1;

        syscon::logger::LogDebug("Loading controller config: '%s' [default] ...", configFullPath.c_str());

        int rc = ini_parse_string(contents.c_str(), ParseControllerConfigLine, &cfg_default);
        if (rc)
            return rc;

        // Override with vendor specific config
        syscon::logger::LogDebug("Loading controller config: '%s' [%s] ...", configFullPath.c_str(), std::string(controllerVidPid).c_str());
        rc = ini_parse_string(contents.c_str(), ParseControllerConfigLine, &cfg_controller);
        if (rc)
            return rc;

        if (!cfg_controller.ini_section_found && auto_add_controller)
        {
            syscon::logger::LogDebug("Controller not found in config file, adding it as '%s'...", default_profile.c_str());
            rc = AddControllerToConfig(configFullPath.c_str(), std::string(controllerVidPid), default_profile);
            if (rc)
                return rc;

            syscon::logger::LogDebug("Reloading controller config: '%s' [%s] ...", configFullPath.c_str(), std::string(controllerVidPid).c_str());
            if (!ReadConfigFile(configFullPath, &contents))
                return -1;

            rc = ini_parse_string(contents.c_str(), ParseControllerConfigLine, &cfg_controller);
            if (rc)
                return rc;
        }

        // Check if have a "profile"
        if (config->profile.length() > 0)
        {
            syscon::logger::LogDebug("Loading controller config: '%s' (Profile: [%s]) ... ", configFullPath.c_str(), config->profile.c_str());
            ConfigINIData cfg_profile(config->profile, config);
            rc = ini_parse_string(contents.c_str(), ParseControllerConfigLine, &cfg_profile);
            if (rc)
                return rc;

            // Re-Override with vendor specific config
            // We are doing this to allow the profile to be overrided by the vendor specific config
            // In other words we would like to have [default] overrided by [profile] overrided by [vid-pid]
            rc = ini_parse_string(contents.c_str(), ParseControllerConfigLine, &cfg_controller);
            if (rc)
                return rc;
        }

        if (config->buttonsPin[GamepadButton::B][0] == 0 && config->buttonsPin[GamepadButton::A][0] == 0 && config->buttonsPin[GamepadButton::Y][0] == 0 && config->buttonsPin[GamepadButton::X][0] == 0)
            syscon::logger::LogError("No buttons configured for this controller [%04x-%04x] - Stick might works but buttons will not work (https://github.com/o0Zz/sys-con/blob/master/doc/Troubleshooting.md)", vendor_id, product_id);
        else
            syscon::logger::LogInfo("Controller successfully loaded (B=%d, A=%d, Y=%d, X=%d, ...) !", config->buttonsPin[GamepadButton::B][0], config->buttonsPin[GamepadButton::A][0], config->buttonsPin[GamepadButton::Y][0], config->buttonsPin[GamepadButton::X][0]);

        return 0;
    }
} // namespace syscon::config