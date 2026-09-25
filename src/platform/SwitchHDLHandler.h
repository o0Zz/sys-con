#pragma once

#include <switch.h>
#include "IController.h"
#include "SwitchVirtualGamepadHandler.h"
#include "SwitchMotion.h"
#include <cstring>

// Virtual pads through hiddbg's HDLS (HID Device List Setting) API, firmware 7.0.0+.

class SwitchHDLHandlerData
{
public:
    void reset()
    {
        m_hdlHandle.handle = 0;
        memset(&m_deviceInfo, 0, sizeof(m_deviceInfo));
        memset(&m_hdlState, 0, sizeof(m_hdlState));
        m_motion = SwitchMotion{};
        m_lastMotionTick = 0;
    }

    HiddbgHdlsHandle m_hdlHandle;
    HiddbgHdlsDeviceInfo m_deviceInfo;
    HiddbgHdlsState m_hdlState;
    SwitchMotion m_motion;
    u64 m_lastMotionTick;
};

class SwitchHDLHandler : public SwitchVirtualGamepadHandler
{
private:
    SwitchHDLHandlerData m_hdlsData[CONTROLLER_MAX_INPUTS];

protected:
    bool IsControllerAttached(uint16_t input_idx) override;
    Result DetachController(uint16_t input_idx) override;
    Result AttachController(uint16_t input_idx) override;
    Result UpdateControllerState(const SwitchPadState &state, uint16_t input_idx) override;

public:
    SwitchHDLHandler(std::unique_ptr<controllerlib::IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority);
    virtual ~SwitchHDLHandler();

    virtual Result Initialize() override;

    static HiddbgHdlsSessionId &GetHdlsSessionId();
};