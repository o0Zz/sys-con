#include "SwitchMITMHandler.h"
#include "SwitchLogger.h"
#include <cmath>
#include <chrono>

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
using namespace controllerlib;


/******************************************************************************
 * SwitchMITMHandler Implementation
 *****************************************************************************/

SwitchMITMHandler::SwitchMITMHandler(std::unique_ptr<IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority)
    : SwitchVirtualGamepadHandler(std::move(controller), polling_timeout_ms, thread_priority)
{
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
    m_lastRumble[input_idx] = RumbleState{};

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
    if (!IsControllerAttached(input_idx))
        return 0;

    return m_controllerList[input_idx]->Update(buttons, analog_stick_l, analog_stick_r);
}

static bool IsRumbling(const SwitchMITMHandler::RumbleState &rumble)
{
    return rumble.amp_high > 0.0f || rumble.amp_low > 0.0f;
}

/*
    Only on change: a USB write costs a transfer on the same thread that reads the pad, and a
    game keeps resending the same value every frame for as long as the effect lasts.
*/
Result SwitchMITMHandler::UpdateOutput()
{
    if (!m_controller->Support(SUPPORTS_RUMBLE))
        return 0;

    for (uint16_t input_idx = 0; input_idx < m_controller->GetInputCount(); input_idx++)
    {
        if (!IsControllerAttached(input_idx))
            continue;

        RumbleState rumble{};
        m_controllerList[input_idx]->GetRumble(&rumble.amp_high, &rumble.amp_low);

        if (rumble.amp_high == m_lastRumble[input_idx].amp_high && rumble.amp_low == m_lastRumble[input_idx].amp_low)
            continue;

        const bool was_rumbling = IsRumbling(m_lastRumble[input_idx]);
        m_lastRumble[input_idx] = rumble;

        controllerlib::Status rc = m_controller->SetRumble(input_idx, rumble.amp_high, rumble.amp_low);

        // Only the edges: an SD write costs several milliseconds on the very thread that
        // polls the pad, and a game changes the amplitude every frame while it rumbles.
        if (was_rumbling != IsRumbling(rumble))
            syscon::logger::LogInfo("SwitchMITMHandler[%04x-%04x] rumble %s on idx %d (high: %d%%, low: %d%%, rc: %d)",
                                    m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(),
                                    IsRumbling(rumble) ? "started" : "stopped", input_idx,
                                    (int)(rumble.amp_high * 100), (int)(rumble.amp_low * 100), rc);
    }

    return 0;
}
