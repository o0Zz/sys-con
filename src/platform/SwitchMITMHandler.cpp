#include "SwitchMITMHandler.h"
#include "SwitchLogger.h"
#include <algorithm>
#include <cmath>
#include <chrono>

using namespace controllerlib;

namespace
{
    // How long the console may take to publish the npad for a device hiddbg just created.
    constexpr int NpadAppearRetries = 100;
    constexpr u64 NpadAppearDelayNs = 10000000ULL; // 10 ms

    /*
        The npads the real hid is publishing right now. sys-con holds its own hid session
        (main.cpp) and its program id never passes ShouldMitm, so this is the console's real
        view - not the fake shared memory the MITM hands to its clients.
    */
    u32 GetRealNpadMask()
    {
        u32 mask = 0;

        for (u8 i = 0; i < 8; i++)
        {
            if (hidGetNpadStyleSet(static_cast<HidNpadIdType>(i)) != 0)
                mask |= (1u << i);
        }

        return mask;
    }

    /*
        hiddbg hands back a HiddbgHdlsHandle and nothing that names the npad the console gave
        it, so the slot has to be recovered by watching which one appears.
    */
    bool WaitForNewNpad(u32 mask_before, uint8_t *player_idx_out)
    {
        for (int retry = 0; retry < NpadAppearRetries; retry++)
        {
            const u32 appeared = GetRealNpadMask() & ~mask_before;
            if (appeared != 0)
            {
                *player_idx_out = static_cast<uint8_t>(__builtin_ctz(appeared));
                return true;
            }

            svcSleepThread(NpadAppearDelayNs);
        }

        return false;
    }
} // namespace


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

/*
    Holding a player slot is not enough to call the pad attached: the console clears the npad
    it gave us whenever the grip/order screen unassigns the controllers, and only it knows
    that happened - the fake shared memory is ours and would always claim the pad is there.
    Reporting it honestly is what re-arms the L+R re-attach in
    SwitchVirtualGamepadHandler::UpdateInput.
*/
bool SwitchMITMHandler::IsControllerAttached(uint16_t input_idx)
{
    if (m_controllerList[input_idx] == nullptr)
        return false;

    return hidGetNpadStyleSet(static_cast<HidNpadIdType>(m_controllerList[input_idx]->GetPlayerIndex())) != 0;
}

// Hands back everything the pad owns. Safe to call whatever state it is in.
void SwitchMITMHandler::ReleaseController(uint16_t input_idx)
{
    if (m_controllerList[input_idx] != nullptr)
    {
        HidSharedMemoryManager::GetHidSharedMemoryManager().DetachController(m_controllerList[input_idx]);
        m_controllerList[input_idx] = nullptr;
    }

    m_lastRumble[input_idx] = RumbleState{};

    if (m_hdlsHandle[input_idx].handle != 0)
    {
        hiddbgDetachHdlsVirtualDevice(m_hdlsHandle[input_idx]);
        m_hdlsHandle[input_idx].handle = 0;
    }
}

Result SwitchMITMHandler::DetachController(uint16_t input_idx)
{
    ReleaseController(input_idx);

    return 0;
}

/*
    The pad is created through hiddbg first, so hid owns the device and the console announces
    it - player LED, the Controllers screen, and the grip/order screen, none of which look at
    the npad shared memory. Only then is the npad it produced claimed here, so the fake shared
    memory overrides that very slot and the MITM stays the source of the pad's state.
*/
Result SwitchMITMHandler::AttachController(uint16_t input_idx)
{
    if (IsControllerAttached(input_idx))
        return 0;

    // A re-attach arrives here still holding the device the console has already dropped, so
    // give that one back before asking for another.
    ReleaseController(input_idx);

    HiddbgHdlsDeviceInfo deviceInfo;
    BuildHdlsDeviceInfo(&deviceInfo);

    const u32 npad_mask_before = GetRealNpadMask();

    Result rc = hiddbgAttachHdlsVirtualDevice(&m_hdlsHandle[input_idx], &deviceInfo);
    if (R_FAILED(rc))
    {
        syscon::logger::LogError("SwitchMITMHandler[%04x-%04x] Failed to create the device for input: %d (Error: 0x%08X) !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, rc);
        m_hdlsHandle[input_idx].handle = 0;
        return rc;
    }

    uint8_t player_idx = 0;
    if (!WaitForNewNpad(npad_mask_before, &player_idx))
    {
        syscon::logger::LogError("SwitchMITMHandler[%04x-%04x] The console published no npad for the device on input: %d !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx);
        ReleaseController(input_idx);
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    m_controllerList[input_idx] = HidSharedMemoryManager::GetHidSharedMemoryManager().AttachControllerAt(player_idx, deviceInfo.singleColorBody, deviceInfo.singleColorButtons);
    if (m_controllerList[input_idx] == nullptr)
    {
        ReleaseController(input_idx);
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    syscon::logger::LogInfo("SwitchMITMHandler[%04x-%04x] Created the device for input: %d, the console gave it player %d", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, player_idx + 1);

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

    // GetInputCount() can exceed CONTROLLER_MAX_INPUTS on a controller with more endpoints
    // than sys-con tracks (see SwitchVirtualGamepadHandler::UpdateInput).
    const uint16_t input_count = std::min<uint16_t>(m_controller->GetInputCount(), CONTROLLER_MAX_INPUTS);

    for (uint16_t input_idx = 0; input_idx < input_count; input_idx++)
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
