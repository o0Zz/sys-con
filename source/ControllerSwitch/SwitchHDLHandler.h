#pragma once

#include <switch.h>
#include "IController.h"
#include "SwitchVirtualGamepadHandler.h"

// HDLS stands for "HID (Human Interface Devices) Device List Setting".
// It's a part of the Nintendo Switch's HID (Human Interface Devices) system module, which is responsible for handling input from controllers and
// other user interface devices. The HDLS structures and functions are used to manage and manipulate a list of virtual HID devices.

// Wrapper for HDL functions for switch versions [7.0.0+]

class SwitchHDLHandlerData
{
public:
    void reset()
    {
        m_hdlHandle.handle = 0;
        memset(&m_deviceInfo, 0, sizeof(m_deviceInfo));
        memset(&m_hdlState, 0, sizeof(m_hdlState));
    }

    HiddbgHdlsHandle m_hdlHandle;
    HiddbgHdlsDeviceInfo m_deviceInfo;
    HiddbgHdlsState m_hdlState;
};

class SwitchHDLHandler : public SwitchVirtualGamepadHandler
{
private:
    SwitchHDLHandlerData m_hdlsData[CONTROLLER_MAX_INPUTS];

protected:
    bool IsControllerAttached(uint16_t input_idx) override;
    Result DetachController(uint16_t input_idx) override;
    Result AttachController(uint16_t input_idx) override;
    Result UpdateControllerState(u64 buttons, const HidAnalogStickState &analog_stick_l, const HidAnalogStickState &analog_stick_r, uint16_t input_idx) override;

public:
    // Initialize the class with specified controller
    SwitchHDLHandler(std::unique_ptr<IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority);
    virtual ~SwitchHDLHandler();

    // Initialize controller handler, HDL state
    virtual Result Initialize() override;

    static HiddbgHdlsSessionId &GetHdlsSessionId();
};