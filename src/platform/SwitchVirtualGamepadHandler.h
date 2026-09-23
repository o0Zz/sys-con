#pragma once

#include <switch.h>
#include "IGamepad.h"
#include "SwitchVirtualDeviceHandler.h"

class SwitchVirtualGamepadHandlerData
{

public:
    bool m_reattach_controller = false;
    bool m_is_connected = false;
};

// Base class for SwitchHDLHandler (hiddbg) and SwitchMITMHandler (mitm).
class SwitchVirtualGamepadHandler : public SwitchVirtualDeviceHandler
{
protected:
    SwitchVirtualGamepadHandlerData m_controllerData[CONTROLLER_MAX_INPUTS];

    std::unique_ptr<controllerlib::IGamepad> m_controller;

    // Describes the pad to hiddbg. Both handlers create their devices through it, so the
    // description is built once here from the controller's config.
    void BuildHdlsDeviceInfo(HiddbgHdlsDeviceInfo *deviceInfo);

    // Fills out the HDL state with the specified button data and passes it to HID
    virtual bool IsControllerAttached(uint16_t input_idx) = 0;
    virtual Result UpdateControllerState(u64 buttons, const HidAnalogStickState &analog_stick_l, const HidAnalogStickState &analog_stick_r, uint16_t input_idx) = 0;
    virtual Result AttachController(uint16_t input_idx) = 0;
    virtual Result DetachController(uint16_t input_idx) = 0;

    size_t GetInterfaceCount() override { return m_controller->GetDevice()->GetInterfaces().size(); }

public:
    SwitchVirtualGamepadHandler(std::unique_ptr<controllerlib::IGamepad> &&controller, int32_t polling_timeout_ms, int8_t thread_priority = 0x30);
    virtual ~SwitchVirtualGamepadHandler() override;

    // Override this if you want a custom init procedure
    Result Initialize() override;
    // Override this if you want a custom exit procedure
    void Exit() override;

    controllerlib::Status UpdateInput(uint32_t timeout_us) override;
    Result UpdateOutput() override;

    controllerlib::IUSBDevice *GetDevice() override { return m_controller->GetDevice(); }

    static void ConvertAxisToSwitchAxis(float x, float y, int32_t *x_out, int32_t *y_out);
    static u8 ControllerTypeToDeviceType(controllerlib::ControllerType type);

    // Get the raw controller pointer
    inline controllerlib::IGamepad *GetController() { return m_controller.get(); }
};
