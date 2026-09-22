#pragma once

#include "IFileManager.h"

/*
 * The flavour-agnostic sysmodule program: shared bring-up helpers and the application body.
 *
 * The entry point differs per build and is NOT here:
 *   - ams  (ATMOSPHERE=1): libstratosphere owns __appInit/main and calls ams::Main() plus the
 *                          ams::init::* hooks - see src/platform/ams/AmsRuntime.cpp.
 *   - libnx (ATMOSPHERE=0): __appInit/__appExit/main - see src/platform/libnx/LibnxRuntime.cpp.
 * Each of those does only the SM/FS/heap bring-up that genuinely differs, calls the shared
 * helpers below, and hands control to RunApp().
 */
namespace syscon
{
    // Shared bring-up, called from each flavour's system init (where SM is already up).
    void InitializeModules(); // usbHs, pscm
    void FinalizeModules();

    // The application body: config -> controllers -> mode -> USB -> PSC loop -> teardown.
    // fileManager must outlive the call; the logger and the config borrow it.
    void RunApp(IFileManager &fileManager);
} // namespace syscon
