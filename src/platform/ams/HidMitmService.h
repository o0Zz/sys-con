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
            out.SetValue(m_shared_memory_entry->GetSharedMemoryHandle().handle, false /* owned by the shared memory entry, which outlives this copy */);
            R_SUCCEED();
        }

    private:
        std::shared_ptr<HidSharedMemoryEntry> m_shared_memory_entry;
    };
} // namespace ams::syscon::hid::mitm

#define AMS_HID_MITM_ACTIVE_VIBRATION_DEVICE_LIST_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, ActivateVibrationDevice, (u32 vibration_device_handle), (vibration_device_handle))

AMS_SF_DEFINE_INTERFACE(ams::syscon::hid::mitm, IHidMitmActiveVibrationDeviceListInterface, AMS_HID_MITM_ACTIVE_VIBRATION_DEVICE_LIST_INTERFACE_INFO, 0x48494543)

namespace ams::syscon::hid::mitm
{
    /*
     * A game activates every vibration device before sending it a value, and the real hid
     * has no npad in the slots sys-con drives - so activating one of ours has to be answered
     * here. The real list object is kept because a handle naming a real controller still has
     * to reach it.
     */
    class HidMitmActiveVibrationDeviceList : public sf::IServiceObject
    {
    public:
        using Interface = IHidMitmActiveVibrationDeviceListInterface;

    public:
        HidMitmActiveVibrationDeviceList(::Service forward) : m_forward(forward) {}
        virtual ~HidMitmActiveVibrationDeviceList();

        Result ActivateVibrationDevice(u32 vibration_device_handle);

    private:
        ::Service m_forward;
    };
} // namespace ams::syscon::hid::mitm

#define AMS_HID_MITM_INTERFACE_INFO(C, H)                                                                                                                                                                                                                                                                                            \
    AMS_SF_METHOD_INFO(C, H, 0, Result, CreateAppletResource, (sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id), (out, applet_resource_user_id))                                                                                  \
    AMS_SF_METHOD_INFO(C, H, 200, Result, GetVibrationDeviceInfo, (sf::Out<::HidVibrationDeviceInfo> out, u32 vibration_device_handle), (out, vibration_device_handle))                                                                                                                                                               \
    AMS_SF_METHOD_INFO(C, H, 201, Result, SendVibrationValue, (u32 vibration_device_handle, ::HidVibrationValue vibration_value, ams::sf::ClientAppletResourceUserId applet_resource_user_id), (vibration_device_handle, vibration_value, applet_resource_user_id))                         \
    AMS_SF_METHOD_INFO(C, H, 202, Result, GetActualVibrationValue, (sf::Out<::HidVibrationValue> out, u32 vibration_device_handle, ams::sf::ClientAppletResourceUserId applet_resource_user_id), (out, vibration_device_handle, applet_resource_user_id))                                   \
    AMS_SF_METHOD_INFO(C, H, 203, Result, CreateActiveVibrationDeviceList, (sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmActiveVibrationDeviceListInterface>> out), (out))                                                                                                                                               \
    AMS_SF_METHOD_INFO(C, H, 206, Result, SendVibrationValues, (u64 applet_resource_user_id, const sf::InPointerArray<::HidVibrationDeviceHandle> &handles, const sf::InPointerArray<::HidVibrationValue> &values), (applet_resource_user_id, handles, values))                                        \
    AMS_SF_METHOD_INFO(C, H, 211, Result, IsVibrationDeviceMounted, (sf::Out<bool> out, u32 vibration_device_handle, ams::sf::ClientAppletResourceUserId applet_resource_user_id), (out, vibration_device_handle, applet_resource_user_id))

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

        Result CreateAppletResource(sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id);
        Result GetVibrationDeviceInfo(sf::Out<::HidVibrationDeviceInfo> out, u32 vibration_device_handle);
        Result SendVibrationValue(u32 vibration_device_handle, ::HidVibrationValue vibration_value, ams::sf::ClientAppletResourceUserId applet_resource_user_id);
        Result GetActualVibrationValue(sf::Out<::HidVibrationValue> out, u32 vibration_device_handle, ams::sf::ClientAppletResourceUserId applet_resource_user_id);
        Result CreateActiveVibrationDeviceList(sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmActiveVibrationDeviceListInterface>> out);
        Result SendVibrationValues(u64 applet_resource_user_id, const sf::InPointerArray<::HidVibrationDeviceHandle> &handles, const sf::InPointerArray<::HidVibrationValue> &values);
        Result IsVibrationDeviceMounted(sf::Out<bool> out, u32 vibration_device_handle, ams::sf::ClientAppletResourceUserId applet_resource_user_id);
    };

    static_assert(IsIHidMitmInterface<HidMitmService>);
    static_assert(IsIHidMitmAppletResourceInterface<HidMitmAppletResource>);
    static_assert(IsIHidMitmActiveVibrationDeviceListInterface<HidMitmActiveVibrationDeviceList>);

} // namespace ams::syscon::hid::mitm
