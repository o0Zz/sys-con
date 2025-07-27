#include "hid_mitm_service.hpp"
#include "hid_shared_memory.hpp"
#include <stratosphere.hpp>
#include "hid_custom.h"

extern "C" Mutex shmem_mutex;
static std::unordered_map<u64, std::pair<HidSharedMemory *, HidSharedMemory *>> sharedmems;

void add_shmem(u64 pid, SharedMemory *real_shmem, SharedMemory *fake_shmem)
{
    mutexLock(&shmem_mutex);
    HidSharedMemory *real_mapped = (HidSharedMemory *)shmemGetAddr(real_shmem);
    HidSharedMemory *fake_mapped = (HidSharedMemory *)shmemGetAddr(fake_shmem);
    sharedmems[pid] = std::pair<HidSharedMemory *, HidSharedMemory *>(real_mapped, fake_mapped);
    mutexUnlock(&shmem_mutex);
}

void del_shmem(u64 pid)
{
    mutexLock(&shmem_mutex);
    if (sharedmems.find(pid) != sharedmems.end())
    {
        sharedmems.erase(pid);
    }
    mutexUnlock(&shmem_mutex);
}

namespace ams::syscon::hid::mitm
{

    namespace
    {
        constexpr sm::ServiceName HidServiceName = sm::ServiceName::Encode("hid");
    }

    // HidMitmService implementation
    HidMitmService::HidMitmService(sm::MitmProcessInfo &client_info, std::shared_ptr<::Service> &&s)
        : sf::MitmServiceImplBase(std::forward<std::shared_ptr<::Service>>(s), client_info),
          m_interception_enabled(false),
          m_injected_buttons(0),
          m_left_stick_x(0),
          m_left_stick_y(0),
          m_right_stick_x(0),
          m_right_stick_y(0),
          m_lock(),
          m_applet_resource(nullptr)
    {
        /* Constructor body */
    }

    Result HidMitmService::CreateAppletResource(sf::SharedPointer<ams::syscon::hid::mitm::HidMitmAppletResource> out, ams::sf::ClientAppletResourceUserId applet_resource_user_id)
    {
        ::Service out_iappletresource;
        ::SharedMemory real_shmem, fake_shmem;

        // This needs to be the first ipc being done since it relies on stuff that libstrato left for us. TODO: Do this properly
        customHidSetup(this->m_forward_service.get(), &out_iappletresource, &real_shmem, &fake_shmem);

        return 0;
    }

    bool HidMitmService::ShouldMitm(const sm::MitmProcessInfo &client_info)
    {
        // We want to MITM all applications that use HID, except the HID system module itself
        return client_info.program_id != ncm::SystemProgramId::Hid;
    }

    Result HidMitmService::InjectButton(u64 button_mask, bool is_pressed)
    {
        std::scoped_lock lk(m_lock);

        /*if (is_pressed)
        {
            m_injected_buttons |= button_mask;
        }
        else
        {
            m_injected_buttons &= ~button_mask;
        }

        // Inject input immediately if we have an applet resource
        if (m_applet_resource && m_interception_enabled)
        {
            m_applet_resource->InjectInputData(m_injected_buttons, m_left_stick_x, m_left_stick_y, m_right_stick_x, m_right_stick_y);
        }
        */
        R_SUCCEED();
    }

    Result HidMitmService::InjectStick(u32 stick_id, s32 x, s32 y)
    {
        std::scoped_lock lk(m_lock);

        if (stick_id == 0)
        { // Left stick
            m_left_stick_x = x;
            m_left_stick_y = y;
        }
        else if (stick_id == 1)
        { // Right stick
            m_right_stick_x = x;
            m_right_stick_y = y;
        }

        // Inject input immediately if we have an applet resource
        /*if (m_applet_resource && m_interception_enabled)
        {
            m_applet_resource->InjectInputData(m_injected_buttons, m_left_stick_x, m_left_stick_y, m_right_stick_x, m_right_stick_y);
        }*/

        R_SUCCEED();
    }

    Result HidMitmService::SetInterceptionEnabled(bool enabled)
    {
        std::scoped_lock lk(m_lock);
        m_interception_enabled = enabled;
        R_SUCCEED();
    }

    // Forward other methods to the real HID service
    /*  Result HidMitmService::ActivateDebugPad(nn::applet::AppletResourceUserId aruid)
      {
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 1, aruid));
      }

      Result HidMitmService::ActivateTouchScreen(nn::applet::AppletResourceUserId aruid)
      {
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 2, aruid));
      }

      Result HidMitmService::ActivateMouse(nn::applet::AppletResourceUserId aruid)
      {
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 3, aruid));
      }

      Result HidMitmService::ActivateKeyboard(nn::applet::AppletResourceUserId aruid)
      {
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 4, aruid));
      }

      Result HidMitmService::ActivateXpad(u32 basic_xpad_id, nn::applet::AppletResourceUserId aruid)
      {
          const struct
          {
              u32 basic_xpad_id;
              nn::applet::AppletResourceUserId aruid;
          } in = {basic_xpad_id, aruid};
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 11, in));
      }

      Result HidMitmService::ActivateJoyXpad(u32 joy_xpad_id)
      {
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 21, joy_xpad_id));
      }

      Result HidMitmService::ActivateSixAxisSensor(u32 sixaxis_sensor_handle, nn::applet::AppletResourceUserId aruid)
      {
          const struct
          {
              u32 sixaxis_sensor_handle;
              nn::applet::AppletResourceUserId aruid;
          } in = {sixaxis_sensor_handle, aruid};
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 31, in));
      }

      Result HidMitmService::ActivateJoySixAxisSensor(u32 joy_sixaxis_sensor_handle)
      {
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 40, joy_sixaxis_sensor_handle));
      }

      Result HidMitmService::StartSixAxisSensor(u32 sixaxis_sensor_handle, nn::applet::AppletResourceUserId aruid)
      {
          const struct
          {
              u32 sixaxis_sensor_handle;
              nn::applet::AppletResourceUserId aruid;
          } in = {sixaxis_sensor_handle, aruid};
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 50, in));
      }

      Result HidMitmService::StopSixAxisSensor(u32 sixaxis_sensor_handle, nn::applet::AppletResourceUserId aruid)
      {
          const struct
          {
              u32 sixaxis_sensor_handle;
              nn::applet::AppletResourceUserId aruid;
          } in = {sixaxis_sensor_handle, aruid};
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 51, in));
      }

      Result HidMitmService::ActivateGesture(u32 basic_gesture_id, nn::applet::AppletResourceUserId aruid)
      {
          const struct
          {
              u32 basic_gesture_id;
              nn::applet::AppletResourceUserId aruid;
          } in = {basic_gesture_id, aruid};
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 66, in));
      }

      Result HidMitmService::SetSupportedNpadStyleSet(u32 supported_styleset, nn::applet::AppletResourceUserId aruid)
      {
          const struct
          {
              u32 supported_styleset;
              nn::applet::AppletResourceUserId aruid;
          } in = {supported_styleset, aruid};
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 100, in));
      }

      Result HidMitmService::GetSupportedNpadStyleSet(sf::Out<u32> out, nn::applet::AppletResourceUserId aruid)
      {
          u32 supported_styleset;
          R_TRY(serviceDispatchInOut(this->forward_service.get(), 101, aruid, supported_styleset));
          out.SetValue(supported_styleset);
          R_SUCCEED();
      }

      Result HidMitmService::SetSupportedNpadIdType(sf::InArray<u8> supported_ids, nn::applet::AppletResourceUserId aruid)
      {
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 102, aruid,
                                     .buffer_attrs = {SfBufferAttr_HipcPointer | SfBufferAttr_In},
                                     .buffers = {{supported_ids.GetPointer(), supported_ids.GetSize()}}, ));
      }

      Result HidMitmService::ActivateNpad(nn::applet::AppletResourceUserId aruid)
      {
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 103, aruid));
      }

      Result HidMitmService::DeactivateNpad(nn::applet::AppletResourceUserId aruid)
      {
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 104, aruid));
      }

      Result HidMitmService::AcquireNpadStyleSetUpdateEventHandle(sf::Out<sf::CopyHandle> out, u32 npad_id, nn::applet::AppletResourceUserId aruid, u64 unknown)
      {
          const struct
          {
              u32 npad_id;
              nn::applet::AppletResourceUserId aruid;
              u64 unknown;
          } in = {npad_id, aruid, unknown};

          Handle event_handle;
          R_TRY(serviceDispatchIn(this->forward_service.get(), 106, in,
                                  .out_handle_attrs = {SfOutHandleAttr_HipcCopy},
                                  .out_handles = &event_handle, ));

          out.SetValue(event_handle);
          R_SUCCEED();
      }

      Result HidMitmService::DisconnectNpad(u32 npad_id, nn::applet::AppletResourceUserId aruid)
      {
          const struct
          {
              u32 npad_id;
              nn::applet::AppletResourceUserId aruid;
          } in = {npad_id, aruid};
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 107, in));
      }

      Result HidMitmService::GetPlayerLedPattern(sf::Out<u64> out, u32 npad_id)
      {
          u64 pattern;
          R_TRY(serviceDispatchInOut(this->forward_service.get(), 108, npad_id, pattern));
          out.SetValue(pattern);
          R_SUCCEED();
      }

      Result HidMitmService::ActivateNpadWithRevision(s32 revision, nn::applet::AppletResourceUserId aruid)
      {
          const struct
          {
              s32 revision;
              nn::applet::AppletResourceUserId aruid;
          } in = {revision, aruid};
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 109, in));
      }

      Result HidMitmService::SetNpadJoyHoldType(u64 hold_type, nn::applet::AppletResourceUserId aruid)
      {
          const struct
          {
              u64 hold_type;
              nn::applet::AppletResourceUserId aruid;
          } in = {hold_type, aruid};
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 120, in));
      }

      Result HidMitmService::GetNpadJoyHoldType(sf::Out<u64> out, nn::applet::AppletResourceUserId aruid)
      {
          u64 hold_type;
          R_TRY(serviceDispatchInOut(this->forward_service.get(), 121, aruid, hold_type));
          out.SetValue(hold_type);
          R_SUCCEED();
      }

      Result HidMitmService::GetVibrationDeviceInfo(sf::Out<u64> out, u32 vibration_device_handle)
      {
          u64 info;
          R_TRY(serviceDispatchInOut(this->forward_service.get(), 200, vibration_device_handle, info));
          out.SetValue(info);
          R_SUCCEED();
      }

      Result HidMitmService::SendVibrationValue(u32 vibration_device_handle, u64 vibration_value, nn::applet::AppletResourceUserId aruid)
      {
          const struct
          {
              u32 vibration_device_handle;
              u64 vibration_value;
              nn::applet::AppletResourceUserId aruid;
          } in = {vibration_device_handle, vibration_value, aruid};
          R_RETURN(serviceDispatchIn(this->forward_service.get(), 201, in));
      }
  */
    void HidMitmService::InjectInputToSharedMemory()
    {
        std::scoped_lock lk(m_lock);

        /*if (m_applet_resource && m_interception_enabled)
        {
            m_applet_resource->InjectInputData(m_injected_buttons, m_left_stick_x, m_left_stick_y, m_right_stick_x, m_right_stick_y);
        }*/
    }

} // namespace ams::syscon::hid::mitm
