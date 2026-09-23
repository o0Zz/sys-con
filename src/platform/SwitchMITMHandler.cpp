#include "SwitchMITMHandler.h"
#include "SwitchHDLHandler.h"
#include "SwitchLogger.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <mutex>

using namespace controllerlib;

namespace
{
    // How long the console may take to publish the npad for a device hiddbg just created.
    constexpr int NpadAppearRetries = 100;
    constexpr u64 NpadAppearDelayNs = 10000000ULL; // 10 ms

    /*
        A rate limit on idle:sys, not the thing that keeps the console awake - that is the pad
        still not being at rest on the next tick. Without it a pad polling at 125 Hz would
        issue an IPC every 8 ms.
    */
    constexpr u64 ActivityReportPeriodNs = 1000000000ULL; // 1 s

    // ~25% of the +-32767 range: far enough out that only a deliberate push counts.
    constexpr int32_t ActivityStickThreshold = 8000;

    /*
        A pad that is not at rest means somebody is there. Reporting on *change* instead would
        miss the case this exists for: a held direction or button produces byte-identical HID
        reports for as long as it is held, so the console would dim under the user's thumb.

        The stick threshold is not tidiness. analogDeadzonePercent defaults to 0 and the
        [network] profile ships with deadzone_x = 0, and outside a deadzone the raw value is
        passed through - so a resting stick's noise never reads exactly zero, and a bare != 0
        test would hold the console awake with nothing touching the pad. Motion is left out
        entirely for the same reason: a pad on a table drifts forever.
    */
    bool IsUserActive(const SwitchPadState &state)
    {
        if (state.buttons != 0)
            return true;

        return std::abs(state.analog_stick_l.x) > ActivityStickThreshold ||
               std::abs(state.analog_stick_l.y) > ActivityStickThreshold ||
               std::abs(state.analog_stick_r.x) > ActivityStickThreshold ||
               std::abs(state.analog_stick_r.y) > ActivityStickThreshold;
    }

    /*
        Two handlers attaching at the same time would each diff the npad mask across the
        other's device and both claim the same slot, so the whole create-then-identify
        window is serialized.
    */
    std::mutex g_attach_mutex;

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
        it, so the slot has to be recovered by watching which one appears. A second npad
        appearing in the same window makes the answer ambiguous, and guessing would have the
        fake shared memory override a slot that belongs to somebody else.
    */
    bool WaitForNewNpad(u32 mask_before, uint8_t *player_idx_out)
    {
        for (int retry = 0; retry < NpadAppearRetries; retry++)
        {
            const u32 appeared = GetRealNpadMask() & ~mask_before;
            if (appeared != 0)
            {
                if ((appeared & (appeared - 1)) != 0)
                    return false;

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
    The npad slot the fake shared memory overrides is the one hid gave to our hiddbg device,
    so the pad exists exactly as long as that device does. hid destroys it on its own - the
    grip/order screen unassigning the controllers, a game refusing another pad, a sleep cycle
    - and only hid knows; the fake shared memory is ours and would always claim the pad is
    there. Asking hiddbg is what tells them apart: an npad style set in that slot only says
    somebody is there, which after a drop can be a real controller the console moved in.

    Answering honestly is also what re-arms the L+R re-attach in
    SwitchVirtualGamepadHandler::UpdateInput, and the slot is given back here so the manager
    thread stops publishing a pad the console no longer has.
*/
bool SwitchMITMHandler::IsControllerAttached(uint16_t input_idx)
{
    if (m_hdlsHandle[input_idx].handle == 0)
        return false;

    bool attached = false;
    Result rc = hiddbgIsHdlsVirtualDeviceAttached(SwitchHDLHandler::GetHdlsSessionId(), m_hdlsHandle[input_idx], &attached);
    if (R_SUCCEEDED(rc) && attached)
        return true;

    syscon::logger::LogInfo("SwitchMITMHandler[%04x-%04x] The console dropped the device on input: %d (Error: 0x%08X), releasing player %d", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, rc, m_controllerList[input_idx]->GetPlayerIndex() + 1);

    ReleaseController(input_idx);

    return false;
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
    m_hdlsButtons[input_idx] = 0;

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

    HiddbgHdlsDeviceInfo deviceInfo;
    BuildHdlsDeviceInfo(&deviceInfo);

    std::unique_lock<std::mutex> attach_lock(g_attach_mutex);

    const u32 npad_mask_before = GetRealNpadMask();

    Result rc = hiddbgAttachHdlsVirtualDevice(&m_hdlsHandle[input_idx], &deviceInfo);
    if (R_FAILED(rc))
    {
        syscon::logger::LogError("SwitchMITMHandler[%04x-%04x] Failed to create the device for input: %d (Error: 0x%08X) !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, rc);
        m_hdlsHandle[input_idx].handle = 0;
        return rc;
    }

    uint8_t player_idx = 0;
    const bool identified = WaitForNewNpad(npad_mask_before, &player_idx);

    attach_lock.unlock();

    if (!identified)
    {
        syscon::logger::LogError("SwitchMITMHandler[%04x-%04x] The console published no single npad for the device on input: %d !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx);
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

Result SwitchMITMHandler::UpdateControllerState(const SwitchPadState &state, uint16_t input_idx)
{
    if (!IsControllerAttached(input_idx))
        return 0;

    // Read from `state`, before Home and Capture are split off below: pressing either is still
    // a user being present.
    if (IsUserActive(state))
    {
        u64 now = armGetSystemTick();
        if (armTicksToNs(now - m_lastActivityTick) >= ActivityReportPeriodNs)
        {
            m_lastActivityTick = now;

            Result rc = idlesysReportUserIsActive();
            if (R_FAILED(rc))
                syscon::logger::LogError("SwitchMITMHandler idlesysReportUserIsActive failed: 0x%X", rc);
        }
    }

    /*
        Home and Capture only exist as hiddbg HDLS bits; in npad shared memory the same bits
        are the StickLRight/StickLDown pseudo-buttons. They go to the hiddbg device instead,
        so hid raises them through hidsys to am, which the MITM never sees.
    */
    constexpr u64 SystemButtons = HiddbgNpadButton_Home | HiddbgNpadButton_Capture;

    const u64 hdls_buttons = state.buttons & SystemButtons;
    if (hdls_buttons != m_hdlsButtons[input_idx])
    {
        HiddbgHdlsState hdls_state{};
        hdls_state.battery_level = 4;
        hdls_state.buttons = hdls_buttons;

        Result rc = hiddbgSetHdlsState(m_hdlsHandle[input_idx], &hdls_state);
        if (R_FAILED(rc))
            return rc;

        m_hdlsButtons[input_idx] = hdls_buttons;
    }

    SwitchPadState npad_state = state;
    npad_state.buttons &= ~SystemButtons;

    return m_controllerList[input_idx]->Update(npad_state);
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
