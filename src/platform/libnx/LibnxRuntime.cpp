/*
 * libnx (ATMOSPHERE=0) runtime overhead: the libnx application hooks and the entry point.
 * The flavour-agnostic program lives in src/app/main.cpp; this file only does the SM/FS
 * bring-up that libnx does differently, then hands control to syscon::RunApp().
 *
 * NOTE: this is the libnx build only. The Atmosphere build must NOT define __appInit / main;
 * libstratosphere provides those and calls ams::Main() instead (see AmsRuntime.cpp).
 */
#include <switch.h>
#include "main.h"
#include "StdFileManager.h"

// Static, so it is charged to the ~7 MB of system memory Atmosphere leaves for every homebrew
// sysmodule on 21.0.0+. main.cpp logs what is actually used; size it from that.
#define INNER_HEAP_SIZE 0x40000 // 256 KiB

#define R_ABORT_UNLESS(rc)             \
    {                                  \
        if (R_FAILED(rc)) [[unlikely]] \
        {                              \
            diagAbortWithResult(rc);   \
        }                              \
    }

extern "C"
{
    u32 __nx_applet_type = AppletType_None;
    u32 __nx_fs_num_sessions = 1;

    void __libnx_initheap(void)
    {
        static u8 inner_heap[INNER_HEAP_SIZE];
        extern void *fake_heap_start;
        extern void *fake_heap_end;

        fake_heap_start = inner_heap;
        fake_heap_end = inner_heap + sizeof(inner_heap);
    }

    void __appInit(void)
    {
        R_ABORT_UNLESS(smInitialize());

        syscon::InitializeModules(); // usbHs, pscm, firmware version
        R_ABORT_UNLESS(fsInitialize());

        // sm stays open on purpose: RunApp opens hid:dbg only once it has read the config
        // and knows the run is in hiddbg mode, which is long after this point.

        R_ABORT_UNLESS(fsdevMountSdmc());
    }

    void __appExit(void)
    {
        syscon::FinalizeModules();
        fsdevUnmountAll();
        fsExit();
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    syscon::StdFileManager fileManager;
    syscon::RunApp(fileManager);
    return 0;
}
