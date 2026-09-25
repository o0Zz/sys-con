#include "network_module.h"

#include <switch.h>
#include "logger.h"
#include "controller_handler.h"
#include "UdpDevice.h"
#include "drivers/NetworkController.h"

using namespace controllerlib;

namespace syscon::networkpad
{
    namespace
    {
        bool g_enabled = false;
        uint16_t g_port = 0;

        /*
            bsd:u may not be up yet: sys-con is a boot2 sysmodule and starts early. Retry for a
            few seconds, then give up quietly - this is an opt-in testing feature and must never
            be a reason for the console not to finish booting.
        */
        constexpr int SocketInitRetries = 10;
        constexpr s64 SocketInitRetryDelayNs = 500000000LL; // 500 ms, matching svcSleepThread's s64

        // Builds the pad and hands it to the controller handler. Shared by Initialize and OnWake.
        bool CreatePad()
        {
            ControllerConfig config;

            // auto_add_controller is false on purpose: this release ships a [ffff-0001] section,
            // and appending another one to the user's file would be noise, not help.
            if (::syscon::config::LoadControllerConfig(CONFIG_FULLPATH, &config, UdpDevice::VendorId, UdpDevice::ProductId, false, "network") != 0 ||
                config.driver != "network")
            {
                syscon::logger::LogError("NetworkPad: config.ini has no [network] profile - network controller disabled");
                return false;
            }

            auto device = std::make_unique<UdpDevice>(g_port);
            auto controller = std::make_unique<NetworkController>(std::move(device), config,
                                                               std::make_unique<syscon::logger::Logger>());

            // removable=false: RemoveAllNonPlugged() looks for usbHs interface IDs, and this pad
            // has none - it would otherwise be destroyed the next time a real device is plugged
            // or unplugged. See SwitchVirtualGamepadHandler::SetRemovable.
            const Result rc = ::syscon::controllers::Insert(std::move(controller), false);
            if (R_FAILED(rc))
            {
                syscon::logger::LogError("NetworkPad: failed to create the network controller (0x%08X)", rc);
                return false;
            }

            return true;
        }
    } // namespace

    void Initialize(const config::GlobalConfig &globalConfig)
    {
        if (!globalConfig.network_controller)
            return;

        g_port = globalConfig.network_controller_port;

        syscon::logger::LogInfo("Initializing network controller (UDP port %d) ...", g_port);

        Result rc = 0;
        for (int attempt = 0; attempt < SocketInitRetries; attempt++)
        {
            rc = UdpSocketInitialize();
            if (R_SUCCEEDED(rc))
                break;

            svcSleepThread(SocketInitRetryDelayNs);
        }

        if (R_FAILED(rc))
        {
            syscon::logger::LogError("NetworkPad: socket driver unavailable after %d attempts (0x%08X) - network controller disabled", SocketInitRetries, rc);
            return;
        }

        if (!CreatePad())
        {
            UdpSocketFinalize();
            return;
        }

        g_enabled = true;
    }

    void OnWake()
    {
        if (!g_enabled)
            return;

        syscon::logger::LogDebug("NetworkPad: re-creating the network controller after wake ...");

        // The handler, and with it the bound socket, went away with controllers::Clear() on the
        // way into sleep. The socket *driver* is still up, so only the pad has to come back.
        if (!CreatePad())
            syscon::logger::LogError("NetworkPad: could not re-create the network controller after wake");
    }

    void Exit()
    {
        if (!g_enabled)
            return;

        // The handler (and its socket) is destroyed by controllers::Clear(); only the driver is
        // ours to shut down here.
        UdpSocketFinalize();
        g_enabled = false;
    }
} // namespace syscon::networkpad
