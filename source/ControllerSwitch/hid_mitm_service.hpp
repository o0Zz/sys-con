#pragma once

#include "hid_shared_memory.hpp"
#include "hid_mitm_iappletresource.hpp"
#include <stratosphere.hpp>

#include <mutex>
#include <memory>

// Documentation: https://switchbrew.org/wiki/HID_services

#define AMS_HID_MITM_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, CreateAppletResource, (sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id), (out, applet_resource_user_id))

// AMS_SF_METHOD_INFO(C, H, 0, Result, CreateAppletResource, (sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, u64 pid, ams::sf::ClientAppletResourceUserId applet_resource_user_id), (out, pid, applet_resource_user_id))

//
// AMS_SF_METHOD_INFO(C, H, 0, Result, CreateAppletResource, (u64 pid, ams::sf::ClientAppletResourceUserId applet_resource_user_id, sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out), (pid, applet_resource_user_id, out))

AMS_SF_DEFINE_MITM_INTERFACE(ams::syscon::hid::mitm, IHidMitmInterface, AMS_HID_MITM_INTERFACE_INFO, 0x48494444 /* Interface ID for debug, can be anything */)

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

    public:
        using MitmServiceImplBase::MitmServiceImplBase;

    public:
        HidMitmService(std::shared_ptr<::Service> &&s, sm::MitmProcessInfo &client_info);

        static bool ShouldMitm(const sm::MitmProcessInfo &client_info);

        // Service interface methods
        Result CreateAppletResource(sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id);
        // Result CreateAppletResource(u64 pid, ams::sf::ClientAppletResourceUserId applet_resource_user_id, sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out);
        // Result CreateAppletResource(sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, u64 pid, ams::sf::ClientAppletResourceUserId applet_resource_user_id);
    };

    static_assert(IsIHidMitmInterface<HidMitmService>);

} // namespace ams::syscon::hid::mitm
