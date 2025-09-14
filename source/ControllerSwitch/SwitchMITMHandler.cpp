#include "SwitchMITMHandler.h"
#include "SwitchLogger.h"
#include <cmath>
#include <chrono>

/******************************************************************************
 * SwitchMITMHandler Implementation
 *****************************************************************************/

SwitchMITMHandler::SwitchMITMHandler(std::unique_ptr<IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority)
    : SwitchVirtualGamepadHandler(std::move(controller), polling_timeout_ms, thread_priority)
{
}

SwitchMITMHandler::~SwitchMITMHandler()
{
}

Result SwitchMITMHandler::Initialize()
{
    syscon::logger::LogDebug("SwitchMITMHandler[%04x-%04x] Initializing ...", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());

    Result rc = SwitchVirtualGamepadHandler::Initialize();
    if (R_FAILED(rc))
        return rc;

    rc = InitThread();
    if (R_FAILED(rc))
        return rc;

    syscon::logger::LogInfo("SwitchMITMHandler[%04x-%04x] Initialized !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());

    return 0;
}

bool SwitchMITMHandler::IsControllerAttached(uint16_t input_idx)
{
    return m_controllerList[input_idx] != nullptr;
}

Result SwitchMITMHandler::DetachController(uint16_t input_idx)
{
    if (!IsControllerAttached(input_idx))
        return 0;

    HidSharedMemoryManager::GetHidSharedMemoryManager().DetachController(m_controllerList[input_idx]);
    m_controllerList[input_idx] = nullptr;

    return 0;
}

Result SwitchMITMHandler::AttachController(uint16_t input_idx)
{
    if (IsControllerAttached(input_idx))
        return 0;

    m_controllerList[input_idx] = HidSharedMemoryManager::GetHidSharedMemoryManager().AttachController();

    return 0;
}

Result SwitchMITMHandler::UpdateControllerState(u64 buttons, const HidAnalogStickState &analog_stick_l, const HidAnalogStickState &analog_stick_r, uint16_t input_idx)
{
    return m_controllerList[input_idx]->Update(buttons, analog_stick_l, analog_stick_r);
}
