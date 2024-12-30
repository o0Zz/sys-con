#pragma once
#include "ControllerTypes.h"
#include "ControllerConfig.h"
#include "logger.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

#define CONFIG_PATH     "/config/sys-con/"
#define CONFIG_FULLPATH CONFIG_PATH "config.ini"

namespace syscon::config
{
    class ControllerVidPid
    {
    public:
        ControllerVidPid(uint16_t vid, uint16_t pid) : vid(vid), pid(pid) {}

        ControllerVidPid(const std::string &vidpid) : vid(0), pid(0)
        {
            std::size_t delimIdx = vidpid.find('-');
            if (delimIdx != std::string::npos)
            {
                std::string vidStr = vidpid.substr(0, delimIdx);
                std::string pidStr = vidpid.substr(delimIdx + 1);

                vid = strtol(vidStr.c_str(), NULL, 16);
                pid = strtol(pidStr.c_str(), NULL, 16);
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

    class GlobalConfig
    {
    public:
        uint16_t polling_frequency_ms{0};
        s8 polling_thread_priority{0x30};
        int log_level{LOG_LEVEL_INFO};
        DiscoveryMode discovery_mode{DiscoveryMode::HID_AND_XBOX};
        std::vector<ControllerVidPid> discovery_vidpid;
        bool auto_add_controller{true};
    };

    Result LoadGlobalConfig(GlobalConfig *config);

    Result LoadControllerConfig(ControllerConfig *config, uint16_t vendor_id, uint16_t product_id, bool auto_add_controller, const std::string &default_profile);

}; // namespace syscon::config