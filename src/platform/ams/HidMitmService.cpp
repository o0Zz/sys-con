#include "HidMitmService.h"
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

        std::shared_ptr<HidSharedMemoryEntry> entry = HidSharedMemoryManager::GetHidSharedMemoryManager().CreateIfNotExists(this->m_forward_service.get(), applet_resource_user_id.GetValue().value, m_client_info.program_id.value);

        // A half-built entry has no usable shared memory handle; failing the command beats
        // handing the client one that is not there.
        R_UNLESS(entry != nullptr, sf::ResultNotSupported());

        out.SetValue(ams::sf::CreateSharedObjectEmplaced<IHidMitmAppletResourceInterface, HidMitmAppletResource>(entry));

        R_SUCCEED();
    }

    Result HidMitmService::SendVibrationValue(sf::CopyHandle vibration_device_handle, ::HidVibrationValue vibration_value, ams::sf::ClientAppletResourceUserId applet_resource_user_id)
    {
        (void)vibration_device_handle;
        (void)vibration_value;
        ::syscon::logger::LogDebug("HidMitmService::SendVibrationValue...");

        // Find the corresponding shared memory entry
        std::shared_ptr<HidSharedMemoryEntry> entry = HidSharedMemoryManager::GetHidSharedMemoryManager().Get(applet_resource_user_id.GetValue().value, m_client_info.program_id.value);
        if (!entry)
        {
            ::syscon::logger::LogError("HidMitmService::SendVibrationValue: Failed to find shared memory entry");
            // return R_FAILED();
        }

        // Send the vibration value to the shared memory
        // entry->GetSharedMemoryHandle().SendVibrationValue(vibration_device_handle, vibration_value);

        R_SUCCEED();
    }

    bool HidMitmService::ShouldMitm(const sm::MitmProcessInfo &client_info)
    {
        /*
            Everything Nintendo signs - system modules, applets, applications - lives under
            0x01. A program id outside it is a homebrew sysmodule (sys-con itself is 0x69...,
            sys-ftpd and the overlay loader 0x42...): none of them shows a controller, and
            handing one a fake HID shared memory has taken the console down.
        */
        if (client_info.program_id.value < 0x0100000000000000 || client_info.program_id.value > 0x01FFFFFFFFFFFFFF)
        {
            ::syscon::logger::LogDebug("HidMitmService ShouldMitm: 0x%016" PRIx64 " (Sysmodule) ? (no)", client_info.program_id.value);
            return false;
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
