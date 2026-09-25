#pragma once

#include "SwitchVirtualGamepadHandler.h"
#include "SwitchMITMManager.h"

class SwitchMITMHandler : public SwitchVirtualGamepadHandler
{
public:
    struct RumbleState
    {
        float amp_high;
        float amp_low;
    };

private:
    std::array<std::shared_ptr<HidSharedMemoryController>, CONTROLLER_MAX_INPUTS> m_controllerList;
    std::array<RumbleState, CONTROLLER_MAX_INPUTS> m_lastRumble{};

    // The hiddbg device behind each input. It exists so the console announces the pad, and it
    // carries Home and Capture, which the console reads from hid itself and never from the
    // npad the fake shared memory overrides.
    std::array<HiddbgHdlsHandle, CONTROLLER_MAX_INPUTS> m_hdlsHandle{};
    std::array<u64, CONTROLLER_MAX_INPUTS> m_hdlsButtons{};

    u64 m_lastActivityTick = 0;

protected:
    bool IsControllerAttached(uint16_t input_idx) override;
    Result DetachController(uint16_t input_idx) override;
    Result AttachController(uint16_t input_idx) override;
    Result UpdateControllerState(const SwitchPadState &state, uint16_t input_idx) override;

public:
    SwitchMITMHandler(std::unique_ptr<controllerlib::IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority);
    ~SwitchMITMHandler();

    virtual Result Initialize() override;

    // Drains the vibration the MITM stored for our npad slots into the driver.
    virtual Result UpdateOutput() override;

private:
    void ReleaseController(uint16_t input_idx);
};