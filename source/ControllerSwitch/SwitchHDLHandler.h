#pragma once

#include "IController.h"
#include "SwitchVirtualGamepadHandler.h"
#include <switch.h>

// HDLS stands for "HID (Human Interface Devices) Device List Setting".
//  It's a part of the Nintendo Switch's HID (Human Interface Devices) system module, which is responsible for handling input from controllers and
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
    bool m_is_connected;
    bool m_is_sync;
};

class SwitchHDLHandler : public SwitchVirtualGamepadHandler
{
private:
    SwitchHDLHandlerData m_controllerData[CONTROLLER_MAX_INPUTS];

    Result Detach(uint16_t input_idx);
    Result Attach(uint16_t input_idx);

    // Separately init and close the HDL state
    Result InitHdlState();
    Result UninitHdlState();

    // Fills out the HDL state with the specified button data and passes it to HID
    Result UpdateHdlState(const NormalizedButtonData &data, uint16_t input_idx);

public:
    // Initialize the class with specified controller
    SwitchHDLHandler(std::unique_ptr<IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority);
    ~SwitchHDLHandler();

    // Initialize controller handler, HDL state
    virtual Result Initialize() override;
    virtual void Exit() override;

    // This will be called periodically by the input threads
    virtual Result UpdateInput(uint32_t timeout_us) override;
    // This will be called periodically by the output threads
    virtual Result UpdateOutput() override;

    bool IsVirtualDeviceAttached(uint16_t input_idx);

    static HiddbgHdlsSessionId &GetHdlsSessionId();
};