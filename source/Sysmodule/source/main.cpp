#include "switch.h"
#include "logger.h"

#include "usb_module.h"
#include "controller_handler.h"
#include "config_handler.h"
#include "psc_module.h"
#include "version.h"
#include "SwitchHDLHandler.h"

// Size of the inner heap (adjust as necessary).
#define INNER_HEAP_SIZE 0x40000

#define R_ABORT_UNLESS(rc)             \
    {                                  \
        if (R_FAILED(rc)) [[unlikely]] \
        {                              \
            diagAbortWithResult(rc);   \
        }                              \
    }

extern "C"
{
    // Sysmodules should not use applet*.
    u32 __nx_applet_type = AppletType_None;

    // Sysmodules will normally only want to use one FS session.
    u32 __nx_fs_num_sessions = 1;

    // Newlib heap configuration function (makes malloc/free work).
    void __libnx_initheap(void)
    {
        static u8 inner_heap[INNER_HEAP_SIZE];
        extern void *fake_heap_start;
        extern void *fake_heap_end;

        // Configure the newlib heap.
        fake_heap_start = inner_heap;
        fake_heap_end = inner_heap + sizeof(inner_heap);
    }
}

alignas(0x1000) constinit u8 g_hdls_buffer[0x8000];
void *workmem = g_hdls_buffer;
size_t workmem_size = sizeof(g_hdls_buffer);

extern "C" void __appInit(void)
{
    R_ABORT_UNLESS(smInitialize());

    R_ABORT_UNLESS(fsInitialize());

    // R_ABORT_UNLESS(usbHsInitialize());
    R_ABORT_UNLESS(pscmInitialize());
    R_ABORT_UNLESS(setsysInitialize());

    // Initialize system firmware version
    SetSysFirmwareVersion fw;
    R_ABORT_UNLESS(setsysGetFirmwareVersion(&fw));
    hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));

    R_ABORT_UNLESS(hiddbgInitialize());
    /*if (hosversionAtLeast(7, 0, 0))
    {
        workmem = aligned_alloc(0x1000, workmem_size);
        if (!workmem)
            diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));

        R_ABORT_UNLESS(hiddbgAttachHdlsWorkBuffer(&SwitchHDLHandler::GetHdlsSessionId(), workmem, workmem_size));
    }*/

    // smExit();

    R_ABORT_UNLESS(fsdevMountSdmc());
}

extern "C" void __appExit(void)
{
    if (hosversionAtLeast(7, 0, 0))
        hiddbgReleaseHdlsWorkBuffer(SwitchHDLHandler::GetHdlsSessionId());

    hiddbgExit();
    usbHsExit();
    pscmExit();

    fsdevUnmountAll();
    fsExit();
    free(workmem);
}

int main(int argc, char *argv[])
{
    ::syscon::logger::Initialize(CONFIG_PATH "log.log");

    u32 version = hosversionGet();

    ::syscon::logger::LogInfo("-----------------------------------------------------");
    ::syscon::logger::LogInfo("SYS-CON started %s+%d-%s (Build date: %s %s)", ::syscon::version::syscon_tag, ::syscon::version::syscon_commit_count, ::syscon::version::syscon_git_hash, __DATE__, __TIME__);
    ::syscon::logger::LogInfo("OS version: %d.%d.%d", HOSVER_MAJOR(version), HOSVER_MINOR(version), HOSVER_MICRO(version));

    ::syscon::logger::LogDebug("Initializing configuration ...");

    ::syscon::config::GlobalConfig globalConfig;
    ::syscon::config::LoadGlobalConfig(&globalConfig);

    while (1)
    {
        svcSleepThread(1e+8L);
    }
    /*
    ::syscon::logger::SetLogLevel(globalConfig.log_level);

    ::syscon::logger::LogDebug("Initializing controllers ...");
    ::syscon::controllers::Initialize();

    // Reduce polling frequency when we use debug or trace to avoid spamming the logs
    if (globalConfig.log_level == LOG_LEVEL_TRACE && globalConfig.polling_frequency_ms < 500)
        globalConfig.polling_frequency_ms = 500;

    if (globalConfig.log_level == LOG_LEVEL_DEBUG && globalConfig.polling_frequency_ms < 100)
        globalConfig.polling_frequency_ms = 100;

    ::syscon::logger::LogDebug("Polling frequency: %d ms", globalConfig.polling_frequency_ms);
    ::syscon::controllers::SetPollingParameters(globalConfig.polling_frequency_ms, globalConfig.polling_thread_priority);

    ::syscon::logger::LogDebug("Initializing USB stack ...");
    ::syscon::usb::Initialize(globalConfig.discovery_mode, globalConfig.discovery_vidpid, globalConfig.auto_add_controller);

    ::syscon::logger::LogDebug("Initializing power supply managment ...");
    ::syscon::psc::Initialize();

    ::syscon::logger::LogDebug("Initializing signal ...");

    while ((::syscon::psc::IsRunning()))
    {
        svcSleepThread(1e+8L);
    }

    ::syscon::logger::LogDebug("Shutting down sys-con ...");
    ::syscon::psc::Exit();
    ::syscon::usb::Exit();
    ::syscon::controllers::Exit();
    ::syscon::logger::Exit();
    */
}
