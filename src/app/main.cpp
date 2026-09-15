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

namespace syscon
{
    namespace
    {
        // HDLS work buffer for the hiddbg path (mode=hiddbg); file-local so it is not a huge
        // object on RunApp's stack and is shared by both flavours.
        alignas(0x1000) u8 g_hdls_buffer[0x8000]; // 32 KiB

        // libnx's R_ABORT_UNLESS is a macro private to its runtime file, and stratosphere's
        // must not be pulled into this shared file, so use a small helper. diagAbortWithResult
        // is libnx and available in both builds.
        void AbortUnless(Result rc)
        {
            if (R_FAILED(rc)) [[unlikely]]
                diagAbortWithResult(rc);
        }
    } // namespace

    void InitializeModules()
    {
        AbortUnless(hiddbgInitialize()); // opened here; the HDLS work buffer is attached later by RunApp when mode=hiddbg
        AbortUnless(usbHsInitialize());
        AbortUnless(pscmInitialize());
        AbortUnless(pmdmntInitialize());

        // Read the firmware version into libnx's hosversion global. set:sys is opened once
        // for this and intentionally left open: nothing reads it afterward (libstratosphere
        // gets its version from exosphere), and it is released at process exit anyway.
        AbortUnless(setsysInitialize());

        SetSysFirmwareVersion fw;
        AbortUnless(setsysGetFirmwareVersion(&fw));
        hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
    }

    void FinalizeModules()
    {
        pscmExit();
        usbHsExit();
        hiddbgExit();
    }

    void RunApp(FileManagerFactory makeFileManager, BannerFn logExtraBanner)
    {
        ::syscon::logger::Initialize(CONFIG_PATH "log.txt", makeFileManager());

        u32 version = hosversionGet();
        ::syscon::logger::LogInfo("-----------------------------------------------------");
        ::syscon::logger::LogInfo("SYS-CON started %s+%d-%s (Build date: %s %s) - https://github.com/o0Zz/sys-con", version::syscon_tag, version::syscon_commit_count, version::syscon_git_hash, __DATE__, __TIME__);
        ::syscon::logger::LogInfo("OS version: %d.%d.%d", HOSVER_MAJOR(version), HOSVER_MINOR(version), HOSVER_MICRO(version));
        if (logExtraBanner != nullptr)
            logExtraBanner();

        ::syscon::logger::LogDebug("Initializing configuration ...");

        ::syscon::config::GlobalConfig globalConfig;
        ::syscon::config::Initialize(makeFileManager());
        ::syscon::config::LoadGlobalConfig(CONFIG_FULLPATH, &globalConfig);

        ::syscon::logger::SetLogLevel(globalConfig.log_level);

        ::syscon::logger::LogDebug("Initializing controllers ...");
        ::syscon::controllers::Initialize();

        ::syscon::logger::LogDebug("Polling timeout: %d ms", globalConfig.polling_timeout_ms);
        ::syscon::controllers::SetPollingParameters(globalConfig.polling_timeout_ms, globalConfig.polling_thread_priority);
        ::syscon::controllers::SetMode(globalConfig.mode);

        bool hdls_attached = false;
        if (globalConfig.mode == ::syscon::config::VirtualPadMode::MITM)
        {
            ::syscon::logger::LogDebug("Initializing HID MITM (mode=mitm) ...");
            AbortUnless(::syscon::hid::mitm::Initialize());
        }
        else
        {
            ::syscon::logger::LogDebug("Initializing hiddbg HDLS (mode=hiddbg) ...");
            if (hosversionAtLeast(7, 0, 0))
            {
                AbortUnless(hiddbgAttachHdlsWorkBuffer(&::SwitchHDLHandler::GetHdlsSessionId(), &g_hdls_buffer, sizeof(g_hdls_buffer)));
                hdls_attached = true;
            }
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
        else if (hdls_attached && hosversionAtLeast(7, 0, 0))
            hiddbgReleaseHdlsWorkBuffer(::SwitchHDLHandler::GetHdlsSessionId());

        ::syscon::logger::Exit();
    }
} // namespace syscon
