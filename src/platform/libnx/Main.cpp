#include <switch.h>
#include "logger.h"

#include "usb_module.h"
#include "controller_handler.h"
#include "config_handler.h"
#include "psc_module.h"
#include "version.h"
#include "SwitchHDLHandler.h"
#include "HidMitm.h"
#include "StdFileManager.h"

// Size of the inner heap (adjust as necessary).
#define INNER_HEAP_SIZE 0x80000 // 512 KiB

#define R_ABORT_UNLESS(rc)             \
    {                                  \
        if (R_FAILED(rc)) [[unlikely]] \
        {                              \
            diagAbortWithResult(rc);   \
        }                              \
    }

alignas(0x1000) constinit u8 g_hdls_buffer[0x8000]; // 32 KiB

// Set once in main() when the hiddbg HDLS work buffer is attached (mode=hiddbg), so
// __appExit only releases it when it was actually attached.
static bool g_hdls_attached = false;

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

        // Initialize system firmware version
        R_ABORT_UNLESS(setsysInitialize());
        SetSysFirmwareVersion fw;
        R_ABORT_UNLESS(setsysGetFirmwareVersion(&fw));
        hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();

        // Open hid:dbg here (needs sm, which is exited below). The HDLS work buffer is
        // attached later in main(), only when mode=hiddbg, once the config has been read.
        R_ABORT_UNLESS(hiddbgInitialize());

        R_ABORT_UNLESS(usbHsInitialize());
        R_ABORT_UNLESS(pscmInitialize());
        R_ABORT_UNLESS(fsInitialize());

        smExit();

        R_ABORT_UNLESS(fsdevMountSdmc());
    }

    void __appExit(void)
    {
        usbHsExit();
        pscmExit();

        if (g_hdls_attached && hosversionAtLeast(7, 0, 0))
            hiddbgReleaseHdlsWorkBuffer(SwitchHDLHandler::GetHdlsSessionId());

        hiddbgExit();

        fsdevUnmountAll();
        fsExit();
    }
}

int main(int argc, char *argv[])
{
    ::syscon::logger::Initialize(CONFIG_PATH "log.txt", std::make_unique<syscon::StdFileManager>());

    u32 version = hosversionGet();
    ::syscon::logger::LogInfo("-----------------------------------------------------");
    ::syscon::logger::LogInfo("SYS-CON started %s+%d-%s (Build date: %s %s) - https://github.com/o0Zz/sys-con", ::syscon::version::syscon_tag, ::syscon::version::syscon_commit_count, ::syscon::version::syscon_git_hash, __DATE__, __TIME__);
    ::syscon::logger::LogInfo("OS version: %d.%d.%d", HOSVER_MAJOR(version), HOSVER_MINOR(version), HOSVER_MICRO(version));

    ::syscon::logger::LogDebug("Initializing configuration ...");

    ::syscon::config::GlobalConfig globalConfig;
    ::syscon::config::Initialize(std::make_unique<syscon::StdFileManager>());
    ::syscon::config::LoadGlobalConfig(CONFIG_FULLPATH, &globalConfig);

    ::syscon::logger::SetLogLevel(globalConfig.log_level);

    ::syscon::logger::LogDebug("Initializing controllers ...");
    ::syscon::controllers::Initialize();

    ::syscon::logger::LogDebug("Polling timeout: %d ms", globalConfig.polling_timeout_ms);
    ::syscon::controllers::SetPollingParameters(globalConfig.polling_timeout_ms, globalConfig.polling_thread_priority);
    ::syscon::controllers::SetMode(globalConfig.mode);

    if (globalConfig.mode == ::syscon::config::VirtualPadMode::MITM)
    {
        ::syscon::logger::LogDebug("Initializing HID MITM (mode=mitm) ...");
        R_ABORT_UNLESS(::syscon::hid::mitm::Initialize());
    }
    else
    {
        ::syscon::logger::LogDebug("Initializing hiddbg HDLS (mode=hiddbg) ...");
        if (hosversionAtLeast(7, 0, 0))
        {
            R_ABORT_UNLESS(hiddbgAttachHdlsWorkBuffer(&SwitchHDLHandler::GetHdlsSessionId(), &g_hdls_buffer, sizeof(g_hdls_buffer)));
            g_hdls_attached = true;
        }
    }

    ::syscon::logger::LogDebug("Initializing USB stack ...");
    ::syscon::usb::Initialize(globalConfig.discovery_mode, globalConfig.discovery_vidpid, globalConfig.auto_add_controller);

    ::syscon::logger::LogDebug("Initializing power supply managment ...");
    ::syscon::psc::Initialize();

    while ((::syscon::psc::IsRunning()))
    {
        svcSleepThread(1e+8L);
    }

    ::syscon::logger::LogDebug("Shutting down sys-con ...");
    ::syscon::psc::Exit();
    ::syscon::usb::Exit();
    ::syscon::controllers::Exit();

    if (globalConfig.mode == ::syscon::config::VirtualPadMode::MITM)
        ::syscon::hid::mitm::Finalize();

    ::syscon::logger::Exit();
}
