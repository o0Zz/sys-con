#pragma once

#include "switch.h"
#include "IController.h"
#include "SwitchVirtualGamepadHandler.h"
#include "hid_mitm_module.hpp"

#include <memory>
#include <stratosphere.hpp>
#include <vapours.hpp>
#include <stratosphere/sf.hpp>

class SwitchMITMHandlerData
{
public:
    void reset()
    {
    }

    bool m_is_connected;
    bool m_is_sync;
};

class SwitchMITMHandler : public SwitchVirtualGamepadHandler
{
private:
    SwitchMITMHandlerData m_controllerData[CONTROLLER_MAX_INPUTS];

public:
    // Initialize the class with specified controller
    SwitchMITMHandler(std::unique_ptr<IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority);
    ~SwitchMITMHandler();

    // Initialize controller handler, HDL state
    virtual Result Initialize() override;
    virtual void Exit() override;

    // This will be called periodically by the input threads
    virtual Result UpdateInput(uint32_t timeout_us) override;
    // This will be called periodically by the output threads
    virtual Result UpdateOutput() override;

    bool IsVirtualDeviceAttached(uint16_t input_idx);
};