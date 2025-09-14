#pragma once

#include "SwitchVirtualGamepadHandler.h"
#include "SwitchMITMManager.h"

class SwitchMITMHandler : public SwitchVirtualGamepadHandler
{
private:
    std::array<std::shared_ptr<HidSharedMemoryController>, CONTROLLER_MAX_INPUTS> m_controllerList;
    bool IsControllerAttached(uint16_t input_idx);

protected:
    Result DetachController(uint16_t input_idx) override;
    Result AttachController(uint16_t input_idx) override;
    Result UpdateControllerState(u64 buttons, const HidAnalogStickState &analog_stick_l, const HidAnalogStickState &analog_stick_r, uint16_t input_idx) override;

public:
    // Initialize the class with specified controller
    SwitchMITMHandler(std::unique_ptr<IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority);
    ~SwitchMITMHandler();

    // Initialize controller handler, HDL state
    virtual Result Initialize() override;
};