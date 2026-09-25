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

    /*
        A Pro Controller has two linear resonant actuators, left then right; a GameCube
        controller has one eccentric-rotating-mass motor, and a handle made for the Gc style
        (npad style index 8) is asking about that one.
    */
    HidVibrationDeviceInfo GetDeviceInfo(HidVibrationDeviceHandle handle)
    {
        constexpr u32 GcStyleIndex = 8;
        if (handle.npad_style_index == GcStyleIndex)
            return HidVibrationDeviceInfo{.type = HidVibrationDeviceType_GcErm, .position = HidVibrationDevicePosition_None};

        return HidVibrationDeviceInfo{
            .type = HidVibrationDeviceType_LinearResonantActuator,
            .position = handle.device_idx == 0 ? HidVibrationDevicePosition_Left : HidVibrationDevicePosition_Right,
        };
    }
    void StoreGcErm(HidVibrationDeviceHandle handle, u64 command)
    {
        const float amplitude = command == HidVibrationGcErmCommand_Start ? 1.0f : 0.0f;
        Store(handle, HidVibrationValue{.amp_low = amplitude, .freq_low = 160.0f, .amp_high = amplitude, .freq_high = 320.0f});
    }

    bool LoadGcErm(HidVibrationDeviceHandle handle, u64 *command)
    {
        HidVibrationValue value;
        if (!Load(handle, &value))
            return false;

        *command = value.amp_low > 0.0f ? HidVibrationGcErmCommand_Start : HidVibrationGcErmCommand_Stop;
        return true;
    }
} // namespace syscon::hid::mitm::vibration
