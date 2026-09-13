#pragma once

#include "logger.h"
#include "ControllerTypes.h"
#include "ControllerConfig.h"
#include <string>
#include <sstream>
#include <iomanip>
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
            std::stringstream ss;
            ss << std::setfill('0') << std::setw(4) << std::hex << vid;
            ss << "-";
            ss << std::setfill('0') << std::setw(4) << std::hex << pid;
            return ss.str();
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
    // Selected at runtime; both mechanisms are compiled into both build flavours.
    enum class VirtualPadMode
    {
        HIDDBG = 0,
        MITM = 1,
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
    };

    int Initialize(std::unique_ptr<IFileManager> &&fileManager);

    int LoadGlobalConfig(const std::string &configFullPath, GlobalConfig *config);

    int LoadControllerConfig(const std::string &configFullPath, controllerlib::ControllerConfig *config, uint16_t vendor_id, uint16_t product_id, bool auto_add_controller, const std::string &default_profile);

}; // namespace syscon::config