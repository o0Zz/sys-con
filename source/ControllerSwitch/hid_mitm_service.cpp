#include "hid_mitm_service.hpp"
#include "SwitchLogger.h"
#include <stratosphere.hpp>
#include "hid_custom.h"

// https://github.com/Slluxx/switch-sys-tweak/blob/develop/src/ns_srvget_mitm_service.hpp
//

void add_shmem(u64 pid, ::SharedMemory *real_shmem, ::SharedMemory *fake_shmem);

namespace ams::syscon::hid::mitm
{

    namespace
    {
        constexpr sm::ServiceName HidServiceName = sm::ServiceName::Encode("hid");
    }

    // HidMitmService implementation
    HidMitmService::HidMitmService(std::shared_ptr<::Service> &&s, sm::MitmProcessInfo &client_info)
        : sf::MitmServiceImplBase(std::forward<std::shared_ptr<::Service>>(s), client_info),
          m_interception_enabled(false),
          m_injected_buttons(0),
          m_left_stick_x(0),
          m_left_stick_y(0),
          m_right_stick_x(0),
          m_right_stick_y(0),
          m_lock()
    {
        ::syscon::logger::LogDebug("HidMitmService creation for PID: 0x%016" PRIx64, client_info.program_id.value);
    }

    Result HidMitmService::CreateAppletResource(sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id)
    {
        ::syscon::logger::LogDebug("HidMitmService::CreateAppletResource...");

        auto intfInterface = ams::sf::CreateSharedObjectEmplaced<IHidMitmAppletResourceInterface, HidMitmAppletResource>();

        // HidMitmAppletResource *tmp = ((HidMitmAppletResource *));

        // This needs to be the first ipc being done since it relies on stuff that libstrato left for us. TODO: Do this properly
        customHidSetup(this->m_forward_service.get(), &intfInterface.GetImpl().m_appletresource_handle, &intfInterface.GetImpl().m_original_shared_memory, &intfInterface.GetImpl().m_fake_shared_memory);

        //::syscon::logger::LogDebug("HidMitmService::CreateAppletResource: AppletResource: %p,  Handle: %016" PRIx64 ", Size: %zu, Permissions: %u, MapAddr: %p...", intfInterface.GetImpl(), intfInterface.GetImpl().m_fake_shared_memory.handle, intfInterface.GetImpl().m_fake_shared_memory.size, intfInterface.GetImpl().m_fake_shared_memory.perm, intfInterface.GetImpl().m_fake_shared_memory.map_addr);

        intfInterface.GetImpl().m_pid = applet_resource_user_id.GetValue().value;
        add_shmem(intfInterface.GetImpl().m_pid, &intfInterface.GetImpl().m_original_shared_memory, &intfInterface.GetImpl().m_fake_shared_memory);

        // out = intf;
        out.SetValue(intfInterface);
        return 0;
    }

    bool HidMitmService::ShouldMitm(const sm::MitmProcessInfo &client_info)
    {
        // Ignore this PID at boot to avoid to crash to system immediately
        u64 boot_pid_list[] = {
            0x420000000000000E, // System module
            0x0100000000000045,
            0x0100000000000023,
            0x010000000000000d,
            0x420000000007e51a,
            0x010000000000100c,
            0x0100000000001000,
            // 0x010000000000100d, // Gallery
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
            return false; // Do not MITM the HID system module itself
        }

        ::syscon::logger::LogDebug("HidMitmService ShouldMitm: 0x%016" PRIx64 " ? (yes)", client_info.program_id.value);
        return true;
    }

} // namespace ams::syscon::hid::mitm
