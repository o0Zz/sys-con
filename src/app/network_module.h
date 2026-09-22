#pragma once

#include "config_handler.h"

/*
    The network controller driven over UDP (config.ini: network_controller=1).

    This is the testing counterpart of usb_module: instead of discovering a controller on the
    USB bus, it creates one whose reports arrive as datagrams, and hands it to
    controllers::Insert() like any other. Everything downstream - mapping, deadzones, combos,
    and both virtual-pad handlers - is the ordinary path, which is the point: a test driving
    this pad exercises the same code a real controller does.

    Off unless config.ini turns it on. See tools/networkpad.py for the sender, and
    src/platform/UdpDevice.h for the transport.
*/
namespace syscon::networkpad
{
    // No-op unless globalConfig.network_controller is set. Never fatal: a failure here (no
    // network, bsd:u not up, port already taken) logs and leaves the console without the
    // network pad, rather than taking the sysmodule down with it.
    void Initialize(const config::GlobalConfig &globalConfig);
    void Exit();

    /*
        Re-creates the pad after a sleep/wake cycle.

        Waking is not a USB event, so nothing else brings it back: psc_module calls
        controllers::Clear() on the way into sleep, which destroys every handler including
        this one. Called from the PSC thread.
    */
    void OnWake();
} // namespace syscon::networkpad
