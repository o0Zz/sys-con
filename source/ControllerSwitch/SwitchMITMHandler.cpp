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
    for (int i = 0; i < CONTROLLER_MAX_INPUTS; i++)
        m_controllerData[i].m_is_sync = true;
}

SwitchMITMHandler::~SwitchMITMHandler()
{
    Exit();
}

Result SwitchMITMHandler::Initialize()
{
    syscon::logger::LogDebug("SwitchMITMHandler[%04x-%04x] Initializing ...", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());

    Result rc = SwitchVirtualGamepadHandler::Initialize();
    if (R_FAILED(rc))
        return rc;

    /*rc = InitThread();
    if (R_FAILED(rc))
        return rc;
*/
    syscon::logger::LogInfo("SwitchMITMHandler[%04x-%04x] Initialized !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());

    return 0;
}

void SwitchMITMHandler::Exit()
{
    syscon::logger::LogDebug("SwitchMITMHandler[%04x-%04x] Exiting ...", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());

    SwitchVirtualGamepadHandler::Exit();

    syscon::logger::LogInfo("SwitchMITMHandler[%04x-%04x] Uninitialized !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());
}

bool SwitchMITMHandler::IsVirtualDeviceAttached(uint16_t input_idx)
{
    return true; // m_controllerData[input_idx].m_hdlHandle.handle != 0;
}

Result SwitchMITMHandler::UpdateInput(uint32_t timeout_us)
{
    uint16_t input_idx = 0;
    NormalizedButtonData buttonData = {0};

    Result read_rc = m_controller->ReadInput(&buttonData, &input_idx, timeout_us);

    /*
        Note: We must not return here if readInput fail, because it might have change the ControllerConnected state.
        So, we must check if the controller is connected and detach it if it's not.
        This case happen with wireless Xbox 360 controllers
    */

    if (m_controllerData[input_idx].m_is_connected != m_controller->IsControllerConnected(input_idx)) // State changed ?
    {
        m_controllerData[input_idx].m_is_connected = m_controller->IsControllerConnected(input_idx);
        m_controllerData[input_idx].m_is_sync = true; // Force sync to true in order to immediately attach the controller once re-connected

        /*if (!m_controllerData[input_idx].m_is_connected)
            Detach(input_idx);
        */
    }

    if (R_FAILED(read_rc))
        return read_rc;

    auto startTimer = std::chrono::steady_clock::now();

    // We get the button inputs from the input packet and update the state of our controller
    // UpdateHdlState(buttonData, input_idx);

    s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();
    syscon::logger::LogPerf("SwitchMITMHandler[%04x-%04x] UpdateInput took: %d us for idx: %d !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), execution_time_us, input_idx);

    return 0;
}

Result SwitchMITMHandler::UpdateOutput()
{
    // Vibrations are not supported with HDL
    return 0;
}
