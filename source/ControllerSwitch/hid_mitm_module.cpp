#include "hid_mitm_module.hpp"
#include "hid_mitm_service.hpp"
#include <stratosphere.hpp>
#include "SwitchLogger.h"
#include "hid_custom.h"

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
                    ::syscon::logger::LogDebug("ServerManager::OnNeedsToAccept AcceptMitmImpl done.");
                    R_SUCCEED();
                }
                    AMS_UNREACHABLE_DEFAULT_CASE();
            }
        }

        alignas(ams::os::ThreadStackAlignment) constinit u8 g_mitm_thread_stack[0x1000];
        ams::os::ThreadType g_mitm_thread;

        // Flag to track initialization
        bool g_initialized = false;
    } // namespace

    void HidMitmModule::ThreadFunction(void *arg)
    {
        (void)arg;

        ::syscon::logger::LogDebug("HidMitmModule Thread running (Registering)...");
        R_ABORT_UNLESS(g_server_manager.RegisterMitmServer<HidMitmService>(PortIndex_Mitm, HidMitmServiceName));

        ::syscon::logger::LogDebug("HidMitmModule LoopProcess ...");
        g_server_manager.LoopProcess();
    }

    void InitializeHidMitm()
    {
        ::syscon::logger::LogDebug("HidMitmModule Initializing ...");

        if (g_initialized)
        {
            return;
        }

        customHidInitialize();

        // Create and start the MITM thread
        R_ABORT_UNLESS(ams::os::CreateThread(&g_mitm_thread, HidMitmModule::ThreadFunction, nullptr, g_mitm_thread_stack, 0x1000, 20));

        ::syscon::logger::LogDebug("HidMitmModule Thread created !");
        ams::os::SetThreadNamePointer(&g_mitm_thread, "HidMitmThread");

        ::syscon::logger::LogDebug("HidMitmModule Thread started !");
        ams::os::StartThread(&g_mitm_thread);

        g_initialized = true;
    }

    void FinalizeHidMitm()
    {
        if (!g_initialized)
        {
            return;
        }

        // Wait for the thread to finish and clean up
        ams::os::WaitThread(std::addressof(g_mitm_thread));
        ams::os::DestroyThread(std::addressof(g_mitm_thread));

        g_initialized = false;
    }

} // namespace ams::syscon::hid::mitm
