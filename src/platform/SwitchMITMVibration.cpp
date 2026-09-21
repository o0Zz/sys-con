#include "SwitchMITMVibration.h"
#include "SwitchMITMManager.h"

namespace syscon::hid::mitm::vibration
{
    bool IsOwned(HidVibrationDeviceHandle handle)
    {
        return HidSharedMemoryManager::GetHidSharedMemoryManager().IsPlayerIndexOwned(handle.player_number);
    }

    void Store(HidVibrationDeviceHandle handle, const HidVibrationValue &value)
    {
        if (!IsOwned(handle))
            return;

        HidSharedMemoryManager::GetHidSharedMemoryManager().SetVibration(handle.player_number, handle.device_idx, value);
    }

    bool Load(HidVibrationDeviceHandle handle, HidVibrationValue *out)
    {
        if (!IsOwned(handle))
            return false;

        *out = HidSharedMemoryManager::GetHidSharedMemoryManager().GetVibration(handle.player_number, handle.device_idx);
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
