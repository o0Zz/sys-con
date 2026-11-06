#pragma once

#include "SwitchVirtualGamepadHandler.h"
#include "SwitchMITMManager.h"

class SwitchMITMHandler : public SwitchVirtualGamepadHandler
{
private:
    // Null for a sideways Joy-Con, which has neither motion nor rumble here.
    std::array<std::shared_ptr<HidSharedMemoryController>, CONTROLLER_MAX_INPUTS> m_controllerList;
    std::array<controllerlib::RumbleValue, CONTROLLER_MAX_INPUTS> m_lastRumble{};

    // The hiddbg device behind each input: the pad every client sees, through the real hid.
    std::array<HiddbgHdlsHandle, CONTROLLER_MAX_INPUTS> m_hdlsHandle{};
    std::array<HiddbgHdlsState, CONTROLLER_MAX_INPUTS> m_hdlsState{};

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
    Result PublishInitialState(uint16_t input_idx);
};
