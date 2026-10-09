#pragma once
#include <switch.h>
#include "IUSBDevice.h"

// One plugged device as controller_handler keeps it: a gamepad (SwitchVirtualGamepadHandler)
// or a keyboard or mouse (SwitchKeyboardMouseHandler).
class SwitchDeviceHandler
{
    bool m_removable = true;

public:
    virtual ~SwitchDeviceHandler() = default;

    virtual Result Initialize() = 0;
    virtual controllerlib::IUSBDevice *GetDevice() = 0;

    // A device that did not come from USB has no usbHs interface to be found among the plugged
    // ones, so RemoveAllNonPlugged() must leave it alone.
    inline void SetRemovable(bool removable) { m_removable = removable; }
    inline bool IsRemovable() const { return m_removable; }
};
