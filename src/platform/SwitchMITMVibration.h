#pragma once

#include <switch.h>

/*
 * Vibration routing shared by both MITM flavours (src/platform/libnx/HidMitmServer.cpp and
 * src/platform/ams/HidMitmService.cpp), so the handle decoding and the ownership rule are
 * written once.
 *
 * A HidVibrationDeviceHandle is {npad_style_index, player_number, device_idx, pad}, and
 * player_number is the very index HidSharedMemoryController is attached on - so a vibration
 * command names one of sys-con's virtual pads directly. Handles that are not ours belong to
 * a real controller and the caller must forward the command untouched.
 */
namespace syscon::hid::mitm::vibration
{
    bool IsOwned(HidVibrationDeviceHandle handle);

    void Store(HidVibrationDeviceHandle handle, const HidVibrationValue &value);
    bool Load(HidVibrationDeviceHandle handle, HidVibrationValue *out);

    HidVibrationDeviceInfo GetDeviceInfo(HidVibrationDeviceHandle handle);
} // namespace syscon::hid::mitm::vibration
