#pragma once

#include "logger.h"
#include "ControllerTypes.h"
#include "ControllerConfig.h"
#include <string>
#include <vector>
#include <stdlib.h>

#define CONFIG_PATH     "/config/sys-con/"
#define CONFIG_FULLPATH CONFIG_PATH "config.ini"

namespace syscon::config
{
    class ControllerVidPid
    {
    public:
        ControllerVidPid(uint16_t vendorId, uint16_t productId) : vid(vendorId), pid(productId) {}

        ControllerVidPid(const std::string &vidpid) : vid(0), pid(0)
        {
            std::size_t delimIdx = vidpid.find('-');
            if (delimIdx != std::string::npos)
            {
                std::string vidStr = vidpid.substr(0, delimIdx);
                std::string pidStr = vidpid.substr(delimIdx + 1);

                vid = static_cast<uint16_t>(strtol(vidStr.c_str(), NULL, 16));
                pid = static_cast<uint16_t>(strtol(pidStr.c_str(), NULL, 16));
            }
        }

        operator std::string() const
        {
            static constexpr char kHexDigits[] = "0123456789abcdef";

            std::string out(9, '-');
            for (int nibble = 0; nibble < 4; nibble++)
            {
                out[3 - nibble] = kHexDigits[(vid >> (nibble * 4)) & 0xF];
                out[8 - nibble] = kHexDigits[(pid >> (nibble * 4)) & 0xF];
            }

            return out;
        }

        bool operator==(const ControllerVidPid &other) const
        {
            return vid == other.vid && pid == other.pid;
        }

        uint16_t vid;
        uint16_t pid;
    };

    typedef enum DiscoveryMode
    {
        HID_AND_XBOX = 0,
        VIDPID_AND_XBOX = 1,
        VIDPID = 2,
    } DiscoveryMode;

    // How USB controllers are published to the console as virtual pads.
    //  - HIDDBG: attach virtual devices through hiddbg/HDLS (SwitchHDLHandler).
    //  - MITM:   man-in-the-middle the hid service and feed each game a fake HID
    //            shared memory (SwitchMITMHandler + SwitchMITMManager).
    //  - DISABLED: sys-con exits right after reading the config.
    // Selected at runtime; both mechanisms are compiled into both build flavours.
    enum class VirtualPadMode
    {
        HIDDBG = 0,
        MITM = 1,
        DISABLED = 2,
    };

    class GlobalConfig
    {
    public:
        uint16_t polling_timeout_ms{10};
        int8_t polling_thread_priority{30};
        controllerlib::LogLevel log_level{controllerlib::LogLevel::Info};
        DiscoveryMode discovery_mode{DiscoveryMode::HID_AND_XBOX};
        std::vector<ControllerVidPid> discovery_vidpid;
        bool auto_add_controller{true};
        VirtualPadMode mode{VirtualPadMode::HIDDBG};

        // Network controller driven over UDP, for scripted input during testing. Off by default:
        // it opens a listening socket that anyone on the LAN can push button presses into, and
        // it costs a bsd:u session plus ~12 KiB of transfer memory when on. See
        // src/app/network_module.h and tools/networkpad.py.
        bool network_controller{false};
        uint16_t network_controller_port{56789};
    };

    void Initialize(IFileManager &fileManager);

    // Parses a vibration= value; the format is documented in config.ini.
    bool ParseRumbleTemplate(const char *value, controllerlib::ControllerRumbleConfig *rumble);

    int LoadGlobalConfig(const std::string &configFullPath, GlobalConfig *config);

    int LoadControllerConfig(const std::string &configFullPath, controllerlib::ControllerConfig *config, uint16_t vendor_id, uint16_t product_id, bool auto_add_controller, const std::string &default_profile);

}; // namespace syscon::config