#pragma once

#include "IFileManager.h"
#include <memory>

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
    // Returns a fresh IFileManager per call (the logger and the config each take their own).
    using FileManagerFactory = std::unique_ptr<IFileManager> (*)();

    // Logs flavour-specific banner lines right after the logger is initialized. May be null.
    using BannerFn = void (*)();

    // Shared bring-up, called from each flavour's system init (where SM is already up).
    void InitializeModules(); // hiddbg, usbHs, pscm
    void FinalizeModules();

    // The application body: config -> controllers -> mode -> USB -> PSC loop -> teardown.
    void RunApp(FileManagerFactory makeFileManager, BannerFn logExtraBanner = nullptr);
} // namespace syscon
