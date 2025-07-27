#pragma once

#include "hid_shared_memory.hpp"
#include "hid_mitm_iappletresource.hpp"
#include <stratosphere.hpp>

#include <mutex>
#include <memory>

// Documentation: https://switchbrew.org/wiki/HID_services

#define AMS_HID_MITM_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, CreateAppletResource, (sf::SharedPointer<ams::syscon::hid::mitm::HidMitmAppletResource> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id), (out, applet_resource_user_id))

AMS_SF_DEFINE_MITM_INTERFACE(ams::syscon::hid::mitm, IHidMitmInterface, AMS_HID_MITM_INTERFACE_INFO, 0x48494443)

namespace ams::syscon::hid::mitm
{
    class HidMitmService : public sf::MitmServiceImplBase
    {
    private:
        bool m_interception_enabled;
        u64 m_injected_buttons;
        s32 m_left_stick_x;
        s32 m_left_stick_y;
        s32 m_right_stick_x;
        s32 m_right_stick_y;
        std::mutex m_lock;
        sf::SharedPointer<HidMitmAppletResource> m_applet_resource;

    public:
        HidMitmService(sm::MitmProcessInfo &client_info, std::shared_ptr<::Service> &&s);

        static bool ShouldMitm(const sm::MitmProcessInfo &client_info);

        // Input injection methods
        Result InjectButton(u64 button_mask, bool is_pressed);
        Result InjectStick(u32 stick_id, s32 x, s32 y);
        Result SetInterceptionEnabled(bool enabled);

        // Service interface methods
        Result CreateAppletResource(sf::SharedPointer<ams::syscon::hid::mitm::HidMitmAppletResource> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id);

        // HID service forwarding methods
        /* ams::Result ActivateDebugPad(ams::nn::applet::AppletResourceUserId aruid);
         ams::Result ActivateTouchScreen(ams::nn::applet::AppletResourceUserId aruid);
         ams::Result ActivateMouse(ams::nn::applet::AppletResourceUserId aruid);
         ams::Result ActivateKeyboard(ams::nn::applet::AppletResourceUserId aruid);
         ams::Result ActivateXpad(u32 basic_xpad_id, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result ActivateJoyXpad(u32 joy_xpad_id);
         ams::Result ActivateSixAxisSensor(u32 sixaxis_sensor_handle, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result ActivateJoySixAxisSensor(u32 joy_sixaxis_sensor_handle);
         ams::Result StartSixAxisSensor(u32 sixaxis_sensor_handle, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result StopSixAxisSensor(u32 sixaxis_sensor_handle, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result ActivateGesture(u32 basic_gesture_id, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result SetSupportedNpadStyleSet(u32 supported_styleset, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result GetSupportedNpadStyleSet(ams::sf::Out<u32> out, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result SetSupportedNpadIdType(ams::sf::InArray<u8> supported_ids, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result ActivateNpad(ams::nn::applet::AppletResourceUserId aruid);
         ams::Result DeactivateNpad(ams::nn::applet::AppletResourceUserId aruid);
         ams::Result AcquireNpadStyleSetUpdateEventHandle(ams::sf::Out<ams::sf::CopyHandle> out, u32 npad_id, ams::nn::applet::AppletResourceUserId aruid, u64 unknown);
         ams::Result DisconnectNpad(u32 npad_id, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result GetPlayerLedPattern(ams::sf::Out<u64> out, u32 npad_id);
         ams::Result ActivateNpadWithRevision(s32 revision, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result SetNpadJoyHoldType(u64 hold_type, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result GetNpadJoyHoldType(ams::sf::Out<u64> out, ams::nn::applet::AppletResourceUserId aruid);
         ams::Result GetVibrationDeviceInfo(ams::sf::Out<u64> out, u32 vibration_device_handle);
         ams::Result SendVibrationValue(u32 vibration_device_handle, u64 vibration_value, ams::nn::applet::AppletResourceUserId aruid);
 */
        // Internal methods
        void InjectInputToSharedMemory();
        /*static_assert(sf::IsServiceObject<HidMitmService>);
        static_assert(sf::IsMitmServiceObject<HidMitmService>);*/
    };

    static_assert(IsIHidMitmInterface<HidMitmService>);

} // namespace ams::syscon::hid::mitm
