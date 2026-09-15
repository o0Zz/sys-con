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

        /*
            The mapping the pad falls back on when config.ini has no [network] profile.

            Anyone upgrading sys-con keeps the config.ini already on their SD card, so the
            profile this release ships cannot be assumed to exist. Without it the pad would
            inherit [default], whose pins are all 0 (unmapped) - it would attach, and then no
            button would ever do anything, which is a miserable thing to debug.

            Pin N is GamepadButton N, matching NetworkController::ParseData and the shipped
            [network] profile.
        */
        void ApplyBuiltinMapping(ControllerConfig *config)
        {
            config->driver = "network";

            for (GamepadButton button : AllDigitalButtons)
            {
                config->buttonsPin[button][0] = PinId{static_cast<uint8_t>(button)};
                config->buttonsPin[button][1] = PinId{};
            }

            struct StickBinding
            {
                GamepadButton button;
                float sign;
                AnalogAxis axis;
            };

            // Left stick on X/Y and right stick on Z/Rz, matching what [default] declares.
            constexpr StickBinding bindings[] = {
                {GamepadButton::LSTICK_LEFT, -1.0f, AnalogAxis::X},
                {GamepadButton::LSTICK_RIGHT, +1.0f, AnalogAxis::X},
                {GamepadButton::LSTICK_UP, +1.0f, AnalogAxis::Y},
                {GamepadButton::LSTICK_DOWN, -1.0f, AnalogAxis::Y},
                {GamepadButton::RSTICK_LEFT, -1.0f, AnalogAxis::Z},
                {GamepadButton::RSTICK_RIGHT, +1.0f, AnalogAxis::Z},
                {GamepadButton::RSTICK_UP, +1.0f, AnalogAxis::Rz},
                {GamepadButton::RSTICK_DOWN, -1.0f, AnalogAxis::Rz},
            };

            for (const StickBinding &binding : bindings)
            {
                config->buttonsAnalog[binding.button].sign = binding.sign;
                config->buttonsAnalog[binding.button].bind = binding.axis;
                config->buttonsPin[binding.button][0] = PinId{};
                config->buttonsPin[binding.button][1] = PinId{};
            }

            // The sender already says exactly where the sticks are; shaping them again would
            // only lose precision.
            for (AnalogAxis axis : {AnalogAxis::X, AnalogAxis::Y, AnalogAxis::Z, AnalogAxis::Rz})
            {
                config->analogDeadzonePercent[axis] = 0;
                config->analogFactorPercent[axis] = 100;
            }
        }

        ControllerConfig LoadConfig()
        {
            ControllerConfig config;

            // auto_add_controller is false on purpose: this release ships a [ffff-0001] section,
            // and appending another one to the user's file would be noise, not help.
            const int rc = ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH, &config,
                                                                 UdpDevice::VendorId, UdpDevice::ProductId,
                                                                 false, "network");

            if (rc != 0 || config.driver != "network")
            {
                syscon::logger::LogWarning("NetworkPad: no [network] profile in config.ini - using the built-in mapping. "
                                           "Update config.ini to customise it.");
                ApplyBuiltinMapping(&config);
            }

            return config;
        }

        // Builds the pad and hands it to the controller handler. Shared by Initialize and OnWake.
        bool CreatePad()
        {
            ControllerConfig config = LoadConfig();

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

    bool IsEnabled()
    {
        return g_enabled;
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
