#include "SwitchMITMVibration.h"
#include "SwitchMITMManager.h"

namespace syscon::hid::mitm::vibration
{
    namespace
    {
        std::shared_ptr<HidSharedMemoryController> ControllerFor(HidVibrationDeviceHandle handle)
        {
            return HidSharedMemoryManager::GetHidSharedMemoryManager().GetController(handle.player_number);
        }
    } // namespace

    bool IsOwned(HidVibrationDeviceHandle handle)
    {
        return ControllerFor(handle) != nullptr;
    }

    void Store(HidVibrationDeviceHandle handle, const HidVibrationValue &value)
    {
        std::shared_ptr<HidSharedMemoryController> controller = ControllerFor(handle);
        if (controller == nullptr)
            return;

        controller->SetVibration(handle.device_idx, value);
    }

    bool Load(HidVibrationDeviceHandle handle, HidVibrationValue *out)
    {
        std::shared_ptr<HidSharedMemoryController> controller = ControllerFor(handle);
        if (controller == nullptr)
            return false;

        *out = controller->GetVibration(handle.device_idx);
        return true;
    }

    // The virtual pad is announced as a Pro Controller, whose actuators are linear resonant
    // ones, left then right (SwitchMITMManager HidSharedMemoryController::Initialize).
    HidVibrationDeviceInfo GetDeviceInfo(HidVibrationDeviceHandle handle)
    {
        return HidVibrationDeviceInfo{
            .type = HidVibrationDeviceType_LinearResonantActuator,
            .position = handle.device_idx == 0 ? HidVibrationDevicePosition_Left : HidVibrationDevicePosition_Right,
        };
    }
} // namespace syscon::hid::mitm::vibration
