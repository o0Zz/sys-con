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

    // The hiddbg device behind each input. It exists so the console announces the pad; its
    // state is never driven, because the fake shared memory overrides the npad it created.
    std::array<HiddbgHdlsHandle, CONTROLLER_MAX_INPUTS> m_hdlsHandle{};

protected:
    bool IsControllerAttached(uint16_t input_idx) override;
    Result DetachController(uint16_t input_idx) override;
    Result AttachController(uint16_t input_idx) override;
    Result UpdateControllerState(u64 buttons, const HidAnalogStickState &analog_stick_l, const HidAnalogStickState &analog_stick_r, uint16_t input_idx) override;

public:
    // Initialize the class with specified controller
    SwitchMITMHandler(std::unique_ptr<controllerlib::IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority);
    ~SwitchMITMHandler();

    // Initialize controller handler, HDL state
    virtual Result Initialize() override;

    // Drains the vibration the MITM stored for our npad slots into the driver.
    virtual Result UpdateOutput() override;
};