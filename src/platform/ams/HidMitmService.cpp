#include "HidMitmService.h"
#include "SwitchMITMVibration.h"
#include "SwitchLogger.h"
#include <stratosphere.hpp>

// https://github.com/Slluxx/switch-sys-tweak/blob/develop/src/ns_srvget_mitm_service.hpp
//

namespace ams::syscon::hid::mitm
{

    // HidMitmService implementation
    HidMitmService::HidMitmService(std::shared_ptr<::Service> &&s, sm::MitmProcessInfo &client_info)
        : sf::MitmServiceImplBase(std::forward<std::shared_ptr<::Service>>(s), client_info)
    {
        ::syscon::logger::LogDebug("HidMitmService creation for program id: 0x%016" PRIx64, client_info.program_id.value);
    }

    Result HidMitmService::CreateAppletResource(sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmAppletResourceInterface>> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id)
    {
        ::syscon::logger::LogDebug("HidMitmService::CreateAppletResource...");

        std::shared_ptr<HidSharedMemoryEntry> entry = HidSharedMemoryManager::GetHidSharedMemoryManager().CreateIfNotExists(this->m_forward_service.get(), applet_resource_user_id.GetValue().value, m_client_info.process_id.value, m_client_info.program_id.value);

        /*
         * No entry means the fake shared memory could not be created - typically
         * 0x00010801 (LimitReached) once the system memory pool is tight, which is easy to
         * hit just by opening an applet. Hand the command to the real hid instead of
         * failing it: libstratosphere replays it on the forward session, so the client gets
         * a genuine IAppletResource and merely does not see sys-con's virtual pad. Failing
         * here instead leaves the applet with no HID shared memory at all, which breaks it
         * and takes the console down with it.
         */
        if (entry == nullptr)
        {
            ::syscon::logger::LogWarning("HidMitmService::CreateAppletResource: no shared memory entry, forwarding to the real hid (program 0x%016" PRIx64 ")", m_client_info.program_id.value);
            R_THROW(sm::mitm::ResultShouldForwardToSession());
        }

        out.SetValue(ams::sf::CreateSharedObjectEmplaced<IHidMitmAppletResourceInterface, HidMitmAppletResource>(entry));

        R_SUCCEED();
    }

    namespace
    {
        constexpr u32 HidCmdCreateActiveVibrationDeviceList = 203;

        /*
            A client polls most of the vibration commands at pad rate and an SD write costs
            milliseconds on the MITM thread, so each trace point fires once per boot. That is
            enough for the log to name the last command reached should one of them ever hang.
        */
        bool TraceOnce(u32 key)
        {
            static u32 seen[12] = {};
            static size_t count = 0;

            for (size_t i = 0; i < count; i++)
            {
                if (seen[i] == key)
                    return false;
            }

            if (count < (sizeof(seen) / sizeof(seen[0])))
                seen[count++] = key;

            return true;
        }

        ::HidVibrationDeviceHandle ToVibrationDeviceHandle(u32 type_value)
        {
            ::HidVibrationDeviceHandle handle;
            handle.type_value = type_value;
            return handle;
        }
    } // namespace

    /*
     * Every vibration command below answers only for the npad slots sys-con drives. A handle
     * naming a real controller is handed back to the real hid with ShouldForwardToSession,
     * which replays the original request on the forward session.
     */
    Result HidMitmService::GetVibrationDeviceInfo(sf::Out<::HidVibrationDeviceInfo> out, u32 vibration_device_handle)
    {
        const ::HidVibrationDeviceHandle handle = ToVibrationDeviceHandle(vibration_device_handle);

        if (TraceOnce(200))
            ::syscon::logger::LogInfo("HidMitmService: GetVibrationDeviceInfo (handle 0x%08X, owned: %d)", vibration_device_handle, (int)::syscon::hid::mitm::vibration::IsOwned(handle));

        if (!::syscon::hid::mitm::vibration::IsOwned(handle))
            R_THROW(sm::mitm::ResultShouldForwardToSession());

        out.SetValue(::syscon::hid::mitm::vibration::GetDeviceInfo(handle));
        R_SUCCEED();
    }

    Result HidMitmService::SendVibrationValue(u32 vibration_device_handle, ::HidVibrationValue vibration_value, ams::sf::ClientAppletResourceUserId applet_resource_user_id)
    {
        AMS_UNUSED(applet_resource_user_id);

        const ::HidVibrationDeviceHandle handle = ToVibrationDeviceHandle(vibration_device_handle);

        if (TraceOnce(201))
            ::syscon::logger::LogInfo("HidMitmService: SendVibrationValue (handle 0x%08X, owned: %d, amp %d/%d%%)", vibration_device_handle, (int)::syscon::hid::mitm::vibration::IsOwned(handle), (int)(vibration_value.amp_high * 100), (int)(vibration_value.amp_low * 100));

        if (!::syscon::hid::mitm::vibration::IsOwned(handle))
            R_THROW(sm::mitm::ResultShouldForwardToSession());

        ::syscon::hid::mitm::vibration::Store(handle, vibration_value);
        R_SUCCEED();
    }

    Result HidMitmService::GetActualVibrationValue(sf::Out<::HidVibrationValue> out, u32 vibration_device_handle, ams::sf::ClientAppletResourceUserId applet_resource_user_id)
    {
        AMS_UNUSED(applet_resource_user_id);

        if (TraceOnce(202))
            ::syscon::logger::LogInfo("HidMitmService: GetActualVibrationValue (handle 0x%08X)", vibration_device_handle);

        ::HidVibrationValue value;
        if (!::syscon::hid::mitm::vibration::Load(ToVibrationDeviceHandle(vibration_device_handle), &value))
            R_THROW(sm::mitm::ResultShouldForwardToSession());

        out.SetValue(value);
        R_SUCCEED();
    }

    Result HidMitmService::CreateActiveVibrationDeviceList(sf::Out<sf::SharedPointer<ams::syscon::hid::mitm::IHidMitmActiveVibrationDeviceListInterface>> out)
    {
        if (TraceOnce(203))
            ::syscon::logger::LogInfo("HidMitmService: CreateActiveVibrationDeviceList - forwarding to the real hid ...");

        ::Service forward_list = {};
        R_TRY(serviceDispatch(this->m_forward_service.get(), HidCmdCreateActiveVibrationDeviceList,
                              .out_num_objects = 1,
                              .out_objects = &forward_list, ));

        out.SetValue(ams::sf::CreateSharedObjectEmplaced<IHidMitmActiveVibrationDeviceListInterface, HidMitmActiveVibrationDeviceList>(forward_list));

        if (TraceOnce(1203))
            ::syscon::logger::LogInfo("HidMitmService: CreateActiveVibrationDeviceList hooked (program 0x%016" PRIx64 ")", m_client_info.program_id.value);

        R_SUCCEED();
    }

    /*
     * One command can carry handles for a sys-con pad and for a real controller at once.
     * Ours are picked out here; unless every handle was ours the request is still replayed on
     * the real hid, so the real pads keep rumbling.
     */
    Result HidMitmService::SendVibrationValues(u64 applet_resource_user_id, const sf::InPointerArray<::HidVibrationDeviceHandle> &handles, const sf::InPointerArray<::HidVibrationValue> &values)
    {
        AMS_UNUSED(applet_resource_user_id);

        const size_t count = std::min(handles.GetSize(), values.GetSize());

        if (TraceOnce(206))
            ::syscon::logger::LogInfo("HidMitmService: SendVibrationValues (%zu handles)", count);

        size_t owned = 0;
        for (size_t i = 0; i < count; i++)
        {
            if (!::syscon::hid::mitm::vibration::IsOwned(handles[i]))
                continue;

            ::syscon::hid::mitm::vibration::Store(handles[i], values[i]);
            owned++;
        }

        if (count == 0 || owned != count)
            R_THROW(sm::mitm::ResultShouldForwardToSession());

        R_SUCCEED();
    }

    Result HidMitmService::IsVibrationDeviceMounted(sf::Out<bool> out, u32 vibration_device_handle, ams::sf::ClientAppletResourceUserId applet_resource_user_id)
    {
        AMS_UNUSED(applet_resource_user_id);

        if (TraceOnce(211))
            ::syscon::logger::LogInfo("HidMitmService: IsVibrationDeviceMounted (handle 0x%08X)", vibration_device_handle);

        if (!::syscon::hid::mitm::vibration::IsOwned(ToVibrationDeviceHandle(vibration_device_handle)))
            R_THROW(sm::mitm::ResultShouldForwardToSession());

        out.SetValue(true);
        R_SUCCEED();
    }

    HidMitmActiveVibrationDeviceList::~HidMitmActiveVibrationDeviceList()
    {
        if (serviceIsActive(&m_forward))
            serviceClose(&m_forward);
    }

    Result HidMitmActiveVibrationDeviceList::ActivateVibrationDevice(u32 vibration_device_handle)
    {
        ::HidVibrationDeviceHandle handle;
        handle.type_value = vibration_device_handle;

        if (TraceOnce(1000))
            ::syscon::logger::LogInfo("HidMitmService: ActivateVibrationDevice (handle 0x%08X, owned: %d)", vibration_device_handle, (int)::syscon::hid::mitm::vibration::IsOwned(handle));

        if (::syscon::hid::mitm::vibration::IsOwned(handle))
            R_SUCCEED();

        R_RETURN(serviceDispatchIn(&m_forward, 0, vibration_device_handle));
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
