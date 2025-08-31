#include "hid_mitm_service.hpp"
#include "SwitchLogger.h"
#include <stratosphere.hpp>

// https://github.com/Slluxx/switch-sys-tweak/blob/develop/src/ns_srvget_mitm_service.hpp
//

namespace ams::syscon::hid::mitm
{

    namespace
    {
        constexpr sm::ServiceName HidServiceName = sm::ServiceName::Encode("hid");
    }

    // HidMitmService implementation
    HidMitmService::HidMitmService(std::shared_ptr<::Service> &&s, sm::MitmProcessInfo &client_info)
        : sf::MitmServiceImplBase(std::forward<std::shared_ptr<::Service>>(s), client_info)
    {
        ::syscon::logger::LogDebug("HidMitmService creation for program id: 0x%016" PRIx64, client_info.program_id.value);
    }

    Result HidMitmService::CreateAppletResource(sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id)
    {
        ::syscon::logger::LogDebug("HidMitmService::CreateAppletResource...");

        std::shared_ptr<HidSharedMemoryEntry> entry = std::make_shared<HidSharedMemoryEntry>(this->m_forward_service.get(), applet_resource_user_id.GetValue().value);
        HidSharedMemoryManager::GetHidSharedMemoryManager().Add(entry);

        out.SetValue(ams::sf::CreateSharedObjectEmplaced<IHidMitmAppletResourceInterface, HidMitmAppletResource>(entry));

        R_SUCCEED();
    }

    bool HidMitmService::ShouldMitm(const sm::MitmProcessInfo &client_info)
    {
        // Ignore this PID at boot to avoid to crash to system immediately
        u64 boot_pid_list[] = {
            0x420000000000000E, // sys-ftpd
            //  0x420000000007e51a, // nx-ovlloader //After wakeup only this one is available we need to hook it

            //  0x010000000000100d, // PhotoViewer / Gallery
        };

        for (const u64 &boot_pid : boot_pid_list)
        {
            if (client_info.program_id.value == boot_pid)
            {
                ::syscon::logger::LogDebug("HidMitmService ShouldMitm: 0x%016" PRIx64 " (Boot) ? (no)", client_info.program_id.value);
                return false; // Ignore these PIDs at boot
            }
        }

        if (IsSystemProgramId(client_info.program_id))
        {
            ::syscon::logger::LogDebug("HidMitmService ShouldMitm: 0x%016" PRIx64 " (System) ? (no)", client_info.program_id.value);
            return false; // Do not MITM the system modules
        }

        ::syscon::logger::LogDebug("HidMitmService ShouldMitm: 0x%016" PRIx64 " ? (yes)", client_info.program_id.value);
        return true;
    }

} // namespace ams::syscon::hid::mitm
