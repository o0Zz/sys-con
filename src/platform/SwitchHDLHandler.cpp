#include "SwitchHDLHandler.h"
#include "SwitchLogger.h"
#include <cmath>
#include <chrono>

using namespace controllerlib;

static HiddbgHdlsSessionId g_hdlsSessionId;

SwitchHDLHandler::SwitchHDLHandler(std::unique_ptr<IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority)
    : SwitchVirtualGamepadHandler(std::move(controller), polling_timeout_ms, thread_priority)
{
}

SwitchHDLHandler::~SwitchHDLHandler()
{
    Exit();
}

Result SwitchHDLHandler::Initialize()
{
    Result rc = SwitchVirtualGamepadHandler::Initialize();
    if (R_FAILED(rc))
        return rc;

    syscon::logger::LogDebug("SwitchHDLHandler[%04x-%04x] Initializing HDL state ...", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());

    for (int i = 0; i < m_controller->GetInputCount(); i++)
    {
        m_hdlsData[i].reset();

        syscon::logger::LogDebug("SwitchHDLHandler[%04x-%04x] Initializing HDL device idx: %d (Controller type: %d) ...", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), i, m_controller->GetConfig().controllerType);

        BuildHdlsDeviceInfo(&m_hdlsData[i].m_deviceInfo);

        m_hdlsData[i].m_hdlState.battery_level = 4; // Set battery charge to full.
        m_hdlsData[i].m_hdlState.analog_stick_l.x = 0;
        m_hdlsData[i].m_hdlState.analog_stick_l.y = 0;
        m_hdlsData[i].m_hdlState.analog_stick_r.x = 0;
        m_hdlsData[i].m_hdlState.analog_stick_r.y = 0;
    }

    syscon::logger::LogDebug("SwitchHDLHandler[%04x-%04x] HDL state successfully initialized !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());

    rc = InitThread();
    if (R_FAILED(rc))
        return rc;

    syscon::logger::LogInfo("SwitchHDLHandler[%04x-%04x] Initialized !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());

    return 0;
}

bool SwitchHDLHandler::IsControllerAttached(uint16_t input_idx)
{
    return m_hdlsData[input_idx].m_hdlHandle.handle != 0;
}

Result SwitchHDLHandler::AttachController(uint16_t input_idx)
{
    if (IsControllerAttached(input_idx))
        return 0;

    syscon::logger::LogDebug("SwitchHDLHandler[%04x-%04x] Attaching device for input: %d ...", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx);

    Result rc = hiddbgAttachHdlsVirtualDevice(&m_hdlsData[input_idx].m_hdlHandle, &m_hdlsData[input_idx].m_deviceInfo);
    if (R_FAILED(rc))
    {
        syscon::logger::LogError("SwitchHDLHandler[%04x-%04x] Failed to attach device for input: %d (Error: 0x%08X)!", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, rc);
        return rc;
    }

    syscon::logger::LogDebug("SwitchHDLHandler[%04x-%04x] Attach - Idx: %d", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx);

    return 0;
}

Result SwitchHDLHandler::DetachController(uint16_t input_idx)
{
    if (!IsControllerAttached(input_idx))
        return 0;

    syscon::logger::LogDebug("SwitchHDLHandler[%04x-%04x] Detaching device for input: %d ...", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx);

    hiddbgDetachHdlsVirtualDevice(m_hdlsData[input_idx].m_hdlHandle);
    m_hdlsData[input_idx].m_hdlHandle.handle = 0;

    return 0;
}

// Sets the state of the class's HDL controller to the state stored in class's hdl.state
Result SwitchHDLHandler::UpdateControllerState(u64 buttons, const HidAnalogStickState &analog_stick_l, const HidAnalogStickState &analog_stick_r, uint16_t input_idx)
{
    HiddbgHdlsState *hdlState = &m_hdlsData[input_idx].m_hdlState;

    hdlState->buttons = buttons;
    hdlState->analog_stick_l.x = analog_stick_l.x;
    hdlState->analog_stick_l.y = analog_stick_l.y;
    hdlState->analog_stick_r.x = analog_stick_r.x;
    hdlState->analog_stick_r.y = analog_stick_r.y;

    if (IsControllerAttached(input_idx))
    {
        syscon::logger::LogDebug("SwitchHDLHandler[%04x-%04x] UpdateHdlState - Idx: %d [Button: 0x%016llX LeftX: %d LeftY: %d RightX: %d RightY: %d]", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, (unsigned long long)hdlState->buttons, hdlState->analog_stick_l.x, hdlState->analog_stick_l.y, hdlState->analog_stick_r.x, hdlState->analog_stick_r.y);
        Result rc = hiddbgSetHdlsState(m_hdlsData[input_idx].m_hdlHandle, hdlState);
        if (R_FAILED(rc))
        {
            /*
                The switch will automatically detach the controller if user goes to SYNC menu or if user enter in a game with only 1 controller (i.e: tftpd)
                So, we must detach the controller if we failed to set the HDL state (In order to avoid a desync between the controller and the switch)
                This case might also happen if user plugged a controller and we were not able to attach it (i.e: if the game do no accept more controller)
            */

            syscon::logger::LogError("SwitchHDLHandler[%04x-%04x] UpdateHdlState failed on idx: %d - Error: 0x%08X (Module: 0x%X, Desc: 0x%X) - Detaching controller ...", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, rc, R_MODULE(rc), R_DESCRIPTION(rc));
            DetachController(input_idx);

            return rc;
        }
    }

    return 0;
}

HiddbgHdlsSessionId &SwitchHDLHandler::GetHdlsSessionId()
{
    return g_hdlsSessionId;
}