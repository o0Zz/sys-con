#pragma once
#include <switch.h>

/*
 * HID MITM lifecycle facade, used only when the config `mode` is MITM.
 *
 * Both build flavours provide an implementation behind the same signature so the two
 * Main.cpp files start/stop the MITM identically:
 *   - libnx (ATMOSPHERE=0): hand-written HidMitmServer (src/platform/libnx/HidMitmServer.cpp)
 *   - ams   (ATMOSPHERE=1): libstratosphere HidMitmModule (src/platform/ams/HidMitmModule.cpp)
 *
 * Initialize() also starts the shared HidSharedMemoryManager; Finalize() stops it.
 */
namespace syscon::hid::mitm
{
    Result Initialize();
    void Finalize();
} // namespace syscon::hid::mitm
