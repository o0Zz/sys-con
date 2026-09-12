#pragma once

#include <stratosphere.hpp>
#include "SwitchMITMManager.h"

// Documentation: https://switchbrew.org/wiki/HID_services

#define AMS_HID_MITM_APPLET_RESOURCE_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, GetSharedMemoryHandle, (ams::sf::OutCopyHandle out), (out))

AMS_SF_DEFINE_INTERFACE(ams::syscon::hid::mitm, IHidMitmAppletResourceInterface, AMS_HID_MITM_APPLET_RESOURCE_INTERFACE_INFO, 0x48494542)

namespace ams::syscon::hid::mitm
{
    class HidMitmAppletResource : public sf::IServiceObject
    {
    public:
        using Interface = IHidMitmAppletResourceInterface;

    public:
        HidMitmAppletResource(std::shared_ptr<HidSharedMemoryEntry> entry) : m_shared_memory_entry(entry) {}
        virtual ~HidMitmAppletResource() = default;

        Result GetSharedMemoryHandle(ams::sf::OutCopyHandle out)
        {
            out.SetValue(m_shared_memory_entry->GetSharedMemoryHandle().handle, false /*AMS need to manage it ?*/);
            R_SUCCEED();
        }

    private:
        std::shared_ptr<HidSharedMemoryEntry> m_shared_memory_entry;
    };
} // namespace ams::syscon::hid::mitm

#define AMS_HID_MITM_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, CreateAppletResource, (sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id), (out, applet_resource_user_id))
  // AMS_SF_METHOD_INFO(C, H, 201, Result, SendVibrationValue, (sf::CopyHandle vibration_device_handle, ::HidVibrationValue vibration_value, ams::sf::ClientAppletResourceUserId applet_resource_user_id), (vibration_device_handle, vibration_value, applet_resource_user_id))

AMS_SF_DEFINE_MITM_INTERFACE(ams::syscon::hid::mitm, IHidMitmInterface, AMS_HID_MITM_INTERFACE_INFO, 0x48494444)
namespace ams::syscon::hid::mitm
{
    class HidMitmService : public sf::MitmServiceImplBase
    {

    public:
        using MitmServiceImplBase::MitmServiceImplBase;

    public:
        HidMitmService(std::shared_ptr<::Service> &&s, sm::MitmProcessInfo &client_info);

        static bool ShouldMitm(const sm::MitmProcessInfo &client_info);

        // Service interface methods
        Result CreateAppletResource(sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id);
        Result SendVibrationValue(sf::CopyHandle vibration_device_handle, ::HidVibrationValue vibration_value, ams::sf::ClientAppletResourceUserId applet_resource_user_id);
    };

    static_assert(IsIHidMitmInterface<HidMitmService>);
    static_assert(IsIHidMitmAppletResourceInterface<HidMitmAppletResource>);

} // namespace ams::syscon::hid::mitm
