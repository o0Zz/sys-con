#include "HidMitmModule.h"
#include "HidMitmService.h"
#include "HidMitm.h"
#include "SwitchMITMManager.h"
#include <stratosphere.hpp>
#include "SwitchLogger.h"

// Based on https://github.com/ndeadly/MissionControl/blob/master/mc_mitm/source/btm_mitm/btm_mitm_service.hpp

namespace ams::syscon::hid::mitm
{

    namespace
    {
        enum PortIndex
        {
            PortIndex_Mitm,
            PortIndex_Count,
        };

        constexpr sm::ServiceName HidMitmServiceName = sm::ServiceName::Encode("hid");

        constexpr size_t MaxSessions = 30;

        struct ServerOptions
        {
            static constexpr size_t PointerBufferSize = 0x1000;
            static constexpr size_t MaxDomains = 0;
            static constexpr size_t MaxDomainObjects = 0;
            static constexpr bool CanDeferInvokeRequest = false;
            static constexpr bool CanManageMitmServers = true;
        };

        class ServerManager final : public sf::hipc::ServerManager<PortIndex_Count, ServerOptions, MaxSessions>
        {
        private:
            virtual Result OnNeedsToAccept(int port_index, Server *server) override;
        };

        ServerManager g_server_manager;

        Result ServerManager::OnNeedsToAccept(int port_index, Server *server)
        {
            /* Acknowledge the mitm session. */
            std::shared_ptr<::Service> fsrv;
            ams::sm::MitmProcessInfo client_info;

            ::syscon::logger::LogDebug("ServerManager::OnNeedsToAccept AcknowledgeMitmSession: %d...", port_index);
            server->AcknowledgeMitmSession(std::addressof(fsrv), std::addressof(client_info));

            switch (port_index)
            {
                case PortIndex_Mitm:
                {
                    ::syscon::logger::LogDebug("ServerManager::OnNeedsToAccept AcceptMitmImpl...");
                    Result ret = this->AcceptMitmImpl(server, sf::CreateSharedObjectEmplaced<IHidMitmInterface, HidMitmService>(decltype(fsrv)(fsrv), client_info), fsrv);
                    if (R_FAILED(ret))
                    {
                        ::syscon::logger::LogError("ServerManager::OnNeedsToAccept AcceptMitmImpl failed: %d", ret);
                        return ret;
                    }
                    R_SUCCEED();
                }
                    AMS_UNREACHABLE_DEFAULT_CASE();
            }
        }

        /*
         * 16 KiB, matching libstratosphere's own mitm query server thread. 4 KiB overflows:
         * libstratosphere's CMIF dispatch is ten frames deep before a handler runs, and
         * HidMitmService::CreateAppletResource then puts the logger's 512-byte line buffer
         * and newlib's vsnprintf on top of that. The console dies with fatal descriptor
         * 0xFFD (StackOverflowErrorDesc) and takes the system down with it.
         */
        alignas(ams::os::ThreadStackAlignment) constinit u8 g_mitm_thread_stack[0x4000];
        ams::os::ThreadType g_mitm_thread;

        bool g_initialized = false;
    } // namespace

    void HidMitmModule::ThreadFunction(void *arg)
    {
        (void)arg;

        ::syscon::logger::LogDebug("HidMitmModule RegisterMitmServer ...");
        R_ABORT_UNLESS(g_server_manager.RegisterMitmServer<HidMitmService>(PortIndex_Mitm, HidMitmServiceName));

        ::syscon::logger::LogDebug("HidMitmModule LoopProcess ...");
        g_server_manager.LoopProcess();
    }

    void InitializeHidMitm()
    {
        ::syscon::logger::LogDebug("HidMitmModule Initializing ...");

        if (g_initialized)
        {
            ::syscon::logger::LogWarning("HidMitmModule already initialized, skipping.");
            return;
        }

        // Create and start the MITM thread
        R_ABORT_UNLESS(ams::os::CreateThread(&g_mitm_thread, HidMitmModule::ThreadFunction, nullptr, g_mitm_thread_stack, sizeof(g_mitm_thread_stack), 20));

        ams::os::SetThreadNamePointer(&g_mitm_thread, "HidMitmThread");

        ams::os::StartThread(&g_mitm_thread);

        g_initialized = true;
    }

    void FinalizeHidMitm()
    {
        if (!g_initialized)
        {
            ::syscon::logger::LogWarning("HidMitmModule not initialized, skipping finalization.");
            return;
        }

        // Wait for the thread to finish and clean up
        ams::os::WaitThread(std::addressof(g_mitm_thread));
        ams::os::DestroyThread(std::addressof(g_mitm_thread));

        g_initialized = false;
    }

} // namespace ams::syscon::hid::mitm

// Shared lifecycle facade (declared in src/platform/HidMitm.h), so both Main.cpp files
// start/stop the MITM identically regardless of build flavour. The libnx build implements
// this in HidMitmServer.cpp; the ams build delegates to the libstratosphere module above.
namespace syscon::hid::mitm
{
    Result Initialize()
    {
        HidSharedMemoryManager::GetHidSharedMemoryManager().Start();
        ams::syscon::hid::mitm::InitializeHidMitm();
        return 0;
    }

    void Finalize()
    {
        HidSharedMemoryManager::GetHidSharedMemoryManager().Stop();
        ams::syscon::hid::mitm::FinalizeHidMitm();
    }
} // namespace syscon::hid::mitm
