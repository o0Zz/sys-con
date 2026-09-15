#include "config_handler.h"

#include "logger.h"
#include "ini.h"

#include <array>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <chrono>

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
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
        std::unique_ptr<IFileManager> file_manager;

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

        GamepadButton stringToButton(const char *name)
        {
            std::string nameStr = convertToLowercase(name);

            if (nameStr == "b")
                return GamepadButton::B;
            else if (nameStr == "a")
                return GamepadButton::A;
            else if (nameStr == "x")
                return GamepadButton::X;
            else if (nameStr == "y")
                return GamepadButton::Y;
            else if (nameStr == "lstick_click")
                return GamepadButton::LSTICK_CLICK;
            else if (nameStr == "lstick_left")
                return GamepadButton::LSTICK_LEFT;
            else if (nameStr == "lstick_right")
                return GamepadButton::LSTICK_RIGHT;
            else if (nameStr == "lstick_up")
                return GamepadButton::LSTICK_UP;
            else if (nameStr == "lstick_down")
                return GamepadButton::LSTICK_DOWN;
            else if (nameStr == "rstick_click")
                return GamepadButton::RSTICK_CLICK;
            else if (nameStr == "rstick_left")
                return GamepadButton::RSTICK_LEFT;
            else if (nameStr == "rstick_right")
                return GamepadButton::RSTICK_RIGHT;
            else if (nameStr == "rstick_up")
                return GamepadButton::RSTICK_UP;
            else if (nameStr == "rstick_down")
                return GamepadButton::RSTICK_DOWN;
            else if (nameStr == "l")
                return GamepadButton::L;
            else if (nameStr == "r")
                return GamepadButton::R;
            else if (nameStr == "zl")
                return GamepadButton::ZL;
            else if (nameStr == "zr")
                return GamepadButton::ZR;
            else if (nameStr == "minus")
                return GamepadButton::MINUS;
            else if (nameStr == "plus")
                return GamepadButton::PLUS;
            else if (nameStr == "dpad_up")
                return GamepadButton::DPAD_UP;
            else if (nameStr == "dpad_right")
                return GamepadButton::DPAD_RIGHT;
            else if (nameStr == "dpad_down")
                return GamepadButton::DPAD_DOWN;
            else if (nameStr == "dpad_left")
                return GamepadButton::DPAD_LEFT;
            else if (nameStr == "capture")
                return GamepadButton::CAPTURE;
            else if (nameStr == "home")
                return GamepadButton::HOME;

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

        bool stringToAnalogConfig(const std::string &cfg, ControllerAnalogConfig *analogCfg)
        {
            std::string stickcfg = convertToLowercase(cfg);

            analogCfg->bind = AnalogAxis::Unknown;
            analogCfg->sign = stickcfg[0] == '-' ? -1.0f : 1.0f;

            if (stickcfg[0] == '-' || stickcfg[0] == '+')
                stickcfg = stickcfg.substr(1);

            if (stickcfg == "x")
                analogCfg->bind = AnalogAxis::X;
            else if (stickcfg == "y")
                analogCfg->bind = AnalogAxis::Y;
            else if (stickcfg == "z")
                analogCfg->bind = AnalogAxis::Z;
            else if (stickcfg == "rz")
                analogCfg->bind = AnalogAxis::Rz;
            else if (stickcfg == "rx")
                analogCfg->bind = AnalogAxis::Rx;
            else if (stickcfg == "ry")
                analogCfg->bind = AnalogAxis::Ry;
            else if (stickcfg == "slider")
                analogCfg->bind = AnalogAxis::Slider;
            else if (stickcfg == "dial")
                analogCfg->bind = AnalogAxis::Dial;
            else if (stickcfg == "brake")
                analogCfg->bind = AnalogAxis::Brake;
            else if (stickcfg == "accelerator")
                analogCfg->bind = AnalogAxis::Accelerator;
            else if (stickcfg == "none")
                analogCfg->bind = AnalogAxis::Unknown;
            else
                return false;

            return true;
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

            // syscon::logger::LogTrace("Parsing controller config line: %s, %s, %s (expect: %s)", section, name, value, ini_data->ini_section.c_str());
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

        /*
            fgets-shaped line reader over an IFile, with a read-ahead buffer.

            The previous implementation issued one IFile::read() per byte, which is one
            virtual call per byte on the libnx build and one ams::fs::ReadFile syscall per
            byte on the Atmosphere one. Since LoadControllerConfig re-parses the whole file
            once per override layer, a single controller connection cost hundreds of
            thousands of those over the shipped ~60 KB config.

            It also got two edge cases wrong, both fixed here:
              - running out of room in `str` returned nullptr, which inih reads as EOF, so an
                over-long line silently discarded the rest of the file. fgets returns the
                partial line instead and resumes on the next call, which is what inih expects.
              - a final line with no trailing newline was dropped entirely.
        */
        class BufferedIniReader
        {
        public:
            explicit BufferedIniReader(IFile *file)
                : m_file(file)
            {
            }

            char *ReadLine(char *str, int num)
            {
                if (str == nullptr || num <= 1)
                    return nullptr;

                int written = 0;

                while (written < num - 1)
                {
                    if (m_pos == m_len && !Refill())
                        break; // End of file.

                    char c = m_buffer[m_pos++];
                    if (c == '\n')
                    {
                        // Newline is dropped; inih rstrip()s the line anyway.
                        break;
                    }

                    str[written++] = c;
                }

                // Nothing read and nothing buffered: genuine end of file.
                if (written == 0 && m_pos == m_len && m_eof)
                    return nullptr;

                str[written] = '\0';
                return str;
            }

        private:
            bool Refill()
            {
                if (m_eof)
                    return false;

                m_len = m_file->read(m_buffer, sizeof(m_buffer));
                m_pos = 0;

                if (m_len == 0)
                {
                    m_eof = true;
                    return false;
                }

                return true;
            }

            IFile *m_file;
            char m_buffer[512];
            std::size_t m_len{0}; // Valid bytes currently in m_buffer.
            std::size_t m_pos{0}; // Next byte of m_buffer to consume.
            bool m_eof{false};
        };

        char *IniReaderLineByLineCallback(char *str, int num, void *stream)
        {
            return static_cast<BufferedIniReader *>(stream)->ReadLine(str, num);
        }

        int ReadFromConfig(const char *path, ini_handler handler, void *config)
        {
            std::unique_ptr<IFile> file = file_manager->open(path, OpenFlags_Read);
            if (!file)
            {
                syscon::logger::LogError("Unable to open configuration file: '%s' !", path);
                return -1;
            }

            BufferedIniReader reader(file.get());
            return ini_parse_stream(IniReaderLineByLineCallback, &reader, handler, config);
        }

    } // namespace

    int Initialize(std::unique_ptr<IFileManager> &&fileManager)
    {
        file_manager = std::move(fileManager);
        return 0;
    }

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

        if (config->buttonsPin[GamepadButton::B][0] == 0 && config->buttonsPin[GamepadButton::A][0] == 0 && config->buttonsPin[GamepadButton::Y][0] == 0 && config->buttonsPin[GamepadButton::X][0] == 0)
            syscon::logger::LogError("No buttons configured for this controller [%04x-%04x] - Stick might works but buttons will not work (https://github.com/o0Zz/sys-con/blob/master/doc/Troubleshooting.md)", vendor_id, product_id);
        else
            syscon::logger::LogInfo("Controller successfully loaded (B=%d, A=%d, Y=%d, X=%d, ...) !", config->buttonsPin[GamepadButton::B][0], config->buttonsPin[GamepadButton::A][0], config->buttonsPin[GamepadButton::Y][0], config->buttonsPin[GamepadButton::X][0]);

        return 0;
    }
} // namespace syscon::config