#include "config_handler.h"

#include "logger.h"
#include "ini.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdlib>
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

        constexpr struct
        {
            std::string_view name;
            AnalogAxis axis;
        } AnalogAxisNames[] = {
            {"x", AnalogAxis::X},
            {"y", AnalogAxis::Y},
            {"z", AnalogAxis::Z},
            {"rz", AnalogAxis::Rz},
            {"rx", AnalogAxis::Rx},
            {"ry", AnalogAxis::Ry},
            {"slider", AnalogAxis::Slider},
            {"dial", AnalogAxis::Dial},
        };

        AnalogAxis stringToAnalogAxis(std::string_view name, std::string_view prefix)
        {
            if (!name.starts_with(prefix))
                return AnalogAxis::Unknown;

            const std::string_view axis = name.substr(prefix.size());
            for (const auto &entry : AnalogAxisNames)
            {
                if (axis == entry.name)
                    return entry.axis;
            }

            return AnalogAxis::Unknown;
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
            else if (AnalogAxis deadzoneAxis = stringToAnalogAxis(nameStr, "deadzone_"); deadzoneAxis != AnalogAxis::Unknown)
                ini_data->controller_config->analogDeadzonePercent[deadzoneAxis] = atoi(value);
            else if (AnalogAxis factorAxis = stringToAnalogAxis(nameStr, "factor_"); factorAxis != AnalogAxis::Unknown)
                ini_data->controller_config->analogFactorPercent[factorAxis] = atoi(value);
            else if (nameStr == "vibration")
            {
                // Not the previous layer's template: a broken key means this controller has none.
                if (!ParseRumbleTemplate(value, &ini_data->controller_config->rumble))
                    ini_data->controller_config->rumble = ControllerRumbleConfig{};
            }
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

    /*
        The vibration= grammar, documented for users in config.ini:

            template := (byte | repeat | placeholder | ' ')*
            byte     := hexdigit hexdigit
            repeat   := byte '*' decimal        the byte appears <decimal> times in total
            placeholder := 'LL' | 'LLLL' | 'RR' | 'RRRR'   L low frequency motor, R high
                                                           lowercase means little endian
    */
    bool ParseRumbleTemplate(const char *value, ControllerRumbleConfig *rumble)
    {
        ControllerRumbleConfig parsed;
        uint32_t nibble = 0;
        uint32_t hexByteEnd = UINT32_MAX;

        for (const char *cursor = value; *cursor != '\0'; cursor++)
        {
            if (*cursor == ' ')
                continue;

            if (nibble / 2 >= MAX_RUMBLE_PACKET_SIZE)
            {
                syscon::logger::LogError("Invalid vibration template: %s (longer than %d bytes)", value, MAX_RUMBLE_PACKET_SIZE);
                return false;
            }

            if (isxdigit(static_cast<unsigned char>(*cursor)))
            {
                const char upper = static_cast<char>(toupper(static_cast<unsigned char>(*cursor)));
                const uint8_t digit = static_cast<uint8_t>((upper <= '9') ? (upper - '0') : (upper - 'A' + 10));

                if ((nibble % 2) == 0)
                    parsed.packet[nibble / 2] = static_cast<uint8_t>(digit << 4);
                else
                    parsed.packet[nibble / 2] |= digit;

                nibble++;

                if ((nibble % 2) == 0)
                    hexByteEnd = nibble;

                continue;
            }

            if (*cursor == '*')
            {
                char *end = nullptr;
                const long repeat = strtol(cursor + 1, &end, 10);

                /* repeat is bounded before it is added to anything: on the console long is
                   64 bit, so the length check below would be done in signed long and a count
                   near LONG_MAX (what strtol saturates to) would overflow it. */
                if (hexByteEnd != nibble || !isdigit(static_cast<unsigned char>(*(cursor + 1))) || repeat < 1 || repeat > MAX_RUMBLE_PACKET_SIZE)
                {
                    syscon::logger::LogError("Invalid vibration template: %s ('*' repeats the hex byte before it, 1 to %d times)", value, MAX_RUMBLE_PACKET_SIZE);
                    return false;
                }

                const uint8_t repeated = parsed.packet[(nibble / 2) - 1];

                if (((nibble / 2) + repeat - 1) > MAX_RUMBLE_PACKET_SIZE)
                {
                    syscon::logger::LogError("Invalid vibration template: %s (longer than %d bytes)", value, MAX_RUMBLE_PACKET_SIZE);
                    return false;
                }

                for (long i = 1; i < repeat; i++)
                {
                    parsed.packet[nibble / 2] = repeated;
                    nibble += 2;
                }

                cursor = end - 1; // The for loop steps over the character that ended the count.
                continue;
            }

            const char letter = static_cast<char>(toupper(static_cast<unsigned char>(*cursor)));
            if (letter != 'L' && letter != 'R')
            {
                syscon::logger::LogError("Invalid vibration template: %s (unexpected character '%c')", value, *cursor);
                return false;
            }

            const char *runStart = cursor;
            while (*cursor == *runStart)
                cursor++;

            const size_t runLength = cursor - runStart;
            cursor--; // The for loop steps over the character that ended the run.

            if ((runLength != 2 && runLength != 4) || (nibble % 2) != 0)
            {
                syscon::logger::LogError("Invalid vibration template: %s (a motor takes 2 or 4 letters of one case, on a byte boundary)", value);
                return false;
            }

            if (((nibble + runLength) / 2) > MAX_RUMBLE_PACKET_SIZE)
            {
                syscon::logger::LogError("Invalid vibration template: %s (longer than %d bytes)", value, MAX_RUMBLE_PACKET_SIZE);
                return false;
            }

            ControllerRumbleField &field = (letter == 'L') ? parsed.low : parsed.high;
            if (field.size != 0)
            {
                syscon::logger::LogError("Invalid vibration template: %s ('%c' appears twice)", value, *runStart);
                return false;
            }

            field.offset = static_cast<uint8_t>(nibble / 2);
            field.size = static_cast<uint8_t>(runLength / 2);
            field.littleEndian = islower(static_cast<unsigned char>(*runStart)) != 0;
            nibble += runLength;
        }

        if ((nibble % 2) != 0)
        {
            syscon::logger::LogError("Invalid vibration template: %s (odd number of hex digits)", value);
            return false;
        }

        if (parsed.low.size == 0 && parsed.high.size == 0)
        {
            syscon::logger::LogError("Invalid vibration template: %s (no L or R placeholder)", value);
            return false;
        }

        parsed.packetSize = static_cast<uint8_t>(nibble / 2);
        *rumble = parsed;

        return true;
    }

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

        std::unique_ptr<IFile> configFile = file_manager->open(path, (OpenFlags)(OpenFlags_Write | OpenFlags_Append));
        if (!configFile || !configFile->is_open())
        {
            syscon::logger::LogError("Error: Unable to open configuration file: %s", path.c_str());
            return -1; // Replace with appropriate error code.
        }

        char stamp[32];
        SYSCON_SNPRINTF(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d:%02d UTC",
                        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        std::string payload = "\n[" + section + "] ; Automatically added on " + stamp + "\n";
        if (!profile.empty())
            payload += "profile=" + profile + "\n";
        else
            payload += "b=1\na=2\nx=3\ny=4\nl=5\nr=6\nzl=7\nzr=8\nminus=9\nplus=10\ncapture=11\nhome=12\n";
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