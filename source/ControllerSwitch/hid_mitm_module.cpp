#include "hid_mitm_module.hpp"
#include "hid_mitm_service.hpp"
#include <stratosphere.hpp>

/*#include "SwitchMITMHandler.h"


*/
// #include <stratosphere/sf.hpp>

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
            static constexpr size_t PointerBufferSize = sf::hipc::DefaultServerManagerOptions::PointerBufferSize;
            static constexpr size_t MaxDomains = sf::hipc::DefaultServerManagerOptions::MaxDomains;
            static constexpr size_t MaxDomainObjects = sf::hipc::DefaultServerManagerOptions::MaxDomainObjects;
            static constexpr bool CanDeferInvokeRequest = sf::hipc::DefaultServerManagerOptions::CanDeferInvokeRequest;
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
            server->AcknowledgeMitmSession(std::addressof(fsrv), std::addressof(client_info));

            switch (port_index)
            {
                /*case PortIndex_Mitm:
                    return this->AcceptMitmImpl(server, ams::sf::CreateSharedObjectEmplaced<IHidMitmInterface, HidMitmService>(client_info, std::move(fsrv)), fsrv);
                */
                default:
                    return ~0; // Invalid port index
            }
        }

        alignas(ams::os::MemoryPageSize) u8 g_mitm_thread_stack[HidMitmModule::StackSize];

        // Thread for running the MITM service
        ams::os::ThreadType g_mitm_thread;

        // Flag to track initialization
        bool g_initialized = false;
    } // namespace

    void HidMitmModule::ThreadFunction(void *arg)
    {
        /* Wait until initialization is complete. */
        // ams::mitm::WaitInitialized();

        // Wait for the real HID service to become available

        // R_ABORT_UNLESS(ams::sm::WaitService(HidMitmServiceName));

        // Install our MITM service
        /*Handle mitm_port_handle;
        ams::sm::ServiceName service_name = HidMitmServiceName;
        R_ABORT_UNLESS(ams::sm::mitm::InstallMitm(std::addressof(mitm_port_handle), HidMitmServiceName.value));
        */

        // Register our MITM service with the server managerP
        R_ABORT_UNLESS(g_server_manager.RegisterMitmServer<HidMitmService>(PortIndex_Mitm, HidMitmServiceName));

        // Process service requests in a loop
        g_server_manager.LoopProcess();
    }

    void InitializeHidMitm()
    {
        if (g_initialized)
        {
            return;
        }

        // Create and start the MITM thread
        R_ABORT_UNLESS(ams::os::CreateThread(std::addressof(g_mitm_thread),
                                             HidMitmModule::ThreadFunction,
                                             nullptr,
                                             g_mitm_thread_stack,
                                             HidMitmModule::StackSize,
                                             HidMitmModule::ThreadPriority));

        ams::os::SetThreadNamePointer(std::addressof(g_mitm_thread), "HidMitmThread");
        ams::os::StartThread(std::addressof(g_mitm_thread));

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
