#include "main.h"

#include <switch.h>
#include "logger.h"
#include "usb_module.h"
#include "controller_handler.h"
#include "config_handler.h"
#include "psc_module.h"
#include "network_module.h"
#include "version.h"
#include "SwitchHDLHandler.h"
#include "HidMitm.h"
#include "IdleSys.h"

namespace syscon
{
    namespace
    {
        // HDLS work buffer for the hiddbg path (mode=hiddbg); file-local so it is not a huge
        // object on RunApp's stack and is shared by both flavours.
        alignas(0x1000) u8 g_hdls_buffer[0x8000]; // 32 KiB

        bool g_hiddbg_initialized = false;

        // Aborting with a step-tagged result rather than the service's own keeps
        // the failing call identifiable: the compiler merges these into a single
        // diagAbortWithResult, so the stack trace alone cannot say which one it was.
        void AbortStep(Result rc, u32 step)
        {
            if (R_FAILED(rc)) [[unlikely]]
                diagAbortWithResult(MAKERESULT(420, step));
        }
    } // namespace

    void InitializeModules()
    {
        // The firmware version must be set before any other service is opened:
        // libnx picks command ids from it, so with hosversion still 0
        // usbHsInitialize sends the pre-2.0.0 command, usb rejects it and
        // closes the session, and the call fails with SessionClosed.
        // set:sys is opened once for this and intentionally left open: nothing
        // reads it afterward (libstratosphere gets its version from exosphere),
        // and it is released at process exit anyway.
        AbortStep(setsysInitialize(), 4);

        SetSysFirmwareVersion fw;
        AbortStep(setsysGetFirmwareVersion(&fw), 5);
        hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));

        AbortStep(usbHsInitialize(), 2);
        AbortStep(pscmInitialize(), 3);

        // Own session on the real hid (sys-con's program id never passes ShouldMitm, so this
        // is not intercepted). It is what lets a handler read back which npad the console
        // gave a pad it just created - see SwitchMITMHandler::AttachController.
        AbortStep(hidInitialize(), 6);
        // idle:sys is what real hid pokes on every input to keep the console out of
        // dimming / auto-sleep. Since we replace hid, the console never sees the inputs
        // via the normal path, so we report the activity ourselves from the mitm loop.
        AbortStep(idlesysInitialize(), 7);
    }

    void FinalizeModules()
    {
        idlesysExit();
        hidExit();
        pscmExit();
        usbHsExit();
        if (g_hiddbg_initialized)
            hiddbgExit();
    }

    void RunApp(IFileManager &fileManager)
    {
        ::syscon::logger::Initialize(CONFIG_PATH "log.txt", fileManager);

        u32 version = hosversionGet();
        ::syscon::logger::LogInfo("-----------------------------------------------------");
        ::syscon::logger::LogInfo("SYS-CON started %s+%d-%s (Build date: %s %s) - https://github.com/o0Zz/sys-con", version::syscon_tag, version::syscon_commit_count, version::syscon_git_hash, __DATE__, __TIME__);
        ::syscon::logger::LogInfo("OS version: %d.%d.%d", HOSVER_MAJOR(version), HOSVER_MINOR(version), HOSVER_MICRO(version));

        ::syscon::logger::LogDebug("Initializing configuration ...");

        ::syscon::config::GlobalConfig globalConfig;
        ::syscon::config::Initialize(fileManager);
        ::syscon::config::LoadGlobalConfig(CONFIG_FULLPATH, &globalConfig);

        ::syscon::logger::SetLogLevel(globalConfig.log_level);

        ::syscon::logger::LogDebug("Initializing controllers ...");
        ::syscon::controllers::Initialize();

        ::syscon::logger::LogDebug("Polling timeout: %d ms", globalConfig.polling_timeout_ms);
        ::syscon::controllers::SetPollingParameters(globalConfig.polling_timeout_ms, globalConfig.polling_thread_priority);
        ::syscon::controllers::SetMode(globalConfig.mode);

        /*
            Both modes create their pads through hiddbg, because that is what makes hid own
            the device: only then does the console announce it - player LED, the Controllers
            screen, and the grip/order screen. MITM mode goes on to override that same npad
            slot in the shared memory it hands each client, which is where its pad state,
            and everything else it will want to fake, comes from.

            Opened here rather than in InitializeModules because the console has a single
            HDLS session and sys-con should hold it no longer than it runs.
        */
        ::syscon::logger::LogDebug("Initializing hiddbg HDLS ...");
        AbortStep(hiddbgInitialize(), 1);
        g_hiddbg_initialized = true;

        bool hdls_attached = false;
        if (hosversionAtLeast(7, 0, 0))
        {
            AbortStep(hiddbgAttachHdlsWorkBuffer(&::SwitchHDLHandler::GetHdlsSessionId(), &g_hdls_buffer, sizeof(g_hdls_buffer)), 7);
            hdls_attached = true;
        }

        if (globalConfig.mode == ::syscon::config::VirtualPadMode::MITM)
        {
            ::syscon::logger::LogDebug("Initializing HID MITM (mode=mitm) ...");
            AbortStep(::syscon::hid::mitm::Initialize(), 8);
        }

        ::syscon::logger::LogDebug("Initializing USB stack ...");
        ::syscon::usb::Initialize(globalConfig.discovery_mode, globalConfig.discovery_vidpid, globalConfig.auto_add_controller);
        ::syscon::networkpad::Initialize(globalConfig);

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
        ::syscon::networkpad::Exit();

        if (globalConfig.mode == ::syscon::config::VirtualPadMode::MITM)
            ::syscon::hid::mitm::Finalize();

        if (hdls_attached)
            hiddbgReleaseHdlsWorkBuffer(::SwitchHDLHandler::GetHdlsSessionId());

        ::syscon::logger::Exit();
    }
} // namespace syscon
