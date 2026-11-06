#include "SwitchMITMHandler.h"
#include "SwitchLogger.h"
#include <algorithm>
#include <mutex>

using namespace controllerlib;

namespace
{
    // How long the console may take to publish the npad for a device hiddbg just created.
    constexpr int NpadAppearRetries = 100;
    constexpr u64 NpadAppearDelayNs = 10000000ULL; // 10 ms

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
        appearing in the same window makes the answer ambiguous, and guessing would send our
        motion and rumble to a slot that belongs to somebody else.
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
    hid takes the npad back on its own - the grip/order screen unassigning every controller, a
    game refusing another pad, a sleep cycle - without detaching the hiddbg device. sys-con's
    own hid view (main.cpp opens it, and it is never mitm'd) empties that npad the moment hid
    lets go of it, a shared memory read rather than an IPC on every poll.

    Answering honestly is what re-arms the L+R re-attach in
    SwitchVirtualGamepadHandler::UpdateInput, and releasing the slot stops our motion and
    rumble going to a slot the console no longer gives us.
*/
bool SwitchMITMHandler::IsControllerAttached(uint16_t input_idx)
{
    if (m_hdlsHandle[input_idx].handle == 0)
        return false;

    // A Joy-Con claims no slot; hid dropping it surfaces as hiddbgSetHdlsState failing.
    if (m_controllerList[input_idx] == nullptr)
        return true;

    const uint8_t player_idx = m_controllerList[input_idx]->GetPlayerIndex();
    if (hidGetNpadStyleSet(static_cast<HidNpadIdType>(player_idx)) != 0)
        return true;

    syscon::logger::LogInfo("SwitchMITMHandler[%04x-%04x] The console dropped the device on input: %d, releasing player %d", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, player_idx + 1);

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

    m_lastRumble[input_idx] = controllerlib::RumbleValue{};
    m_hdlsState[input_idx] = HiddbgHdlsState{};

    if (m_hdlsHandle[input_idx].handle != 0)
    {
        syscon::logger::LogDebug("SwitchMITMHandler releasing the hiddbg device of input: %d ...", input_idx);
        Result rc = hiddbgDetachHdlsVirtualDevice(m_hdlsHandle[input_idx]);
        syscon::logger::LogDebug("SwitchMITMHandler released the hiddbg device of input: %d (Error: 0x%08X)", input_idx, rc);
        m_hdlsHandle[input_idx].handle = 0;
    }
}

Result SwitchMITMHandler::DetachController(uint16_t input_idx)
{
    ReleaseController(input_idx);

    return 0;
}

/*
    The pad is created through hiddbg, so hid owns the device and every client - mitm'd or not
    - sees it exactly as in mode=hiddbg. The npad it produced is then claimed here, so the MITM
    knows which slot to fill motion into and which vibration handles are ours.
*/
Result SwitchMITMHandler::AttachController(uint16_t input_idx)
{
    if (IsControllerAttached(input_idx))
        return 0;

    HiddbgHdlsDeviceInfo deviceInfo;
    BuildHdlsDeviceInfo(&deviceInfo, input_idx);

    std::unique_lock<std::mutex> attach_lock(g_attach_mutex);

    /*
        A sideways Joy-Con claims no slot of its own until the console assigns it. It still
        takes the attach lock, or its npad appearing would make a concurrent Pro attach's slot
        ambiguous.
    */
    if (IsAttachingAsJoyCon(input_idx))
    {
        Result rc = hiddbgAttachHdlsVirtualDevice(&m_hdlsHandle[input_idx], &deviceInfo);
        if (R_FAILED(rc))
        {
            syscon::logger::LogError("SwitchMITMHandler[%04x-%04x] Failed to create the Joy-Con for input: %d (Error: 0x%08X) !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, rc);
            m_hdlsHandle[input_idx].handle = 0;
            return rc;
        }

        syscon::logger::LogInfo("SwitchMITMHandler[%04x-%04x] Created a sideways Joy-Con for input: %d", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx);
        return PublishInitialState(input_idx);
    }

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

    m_controllerList[input_idx] = HidSharedMemoryManager::GetHidSharedMemoryManager().AttachControllerAt(player_idx, m_controller->Support(SUPPORTS_MOTION));
    if (m_controllerList[input_idx] == nullptr)
    {
        ReleaseController(input_idx);
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    syscon::logger::LogInfo("SwitchMITMHandler[%04x-%04x] Created the device for input: %d, the console gave it player %d", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, player_idx + 1);

    return PublishInitialState(input_idx);
}

/*
    The state is otherwise only sent when the input changes, so a pad attached at rest would
    show an empty battery until it is first touched.
*/
Result SwitchMITMHandler::PublishInitialState(uint16_t input_idx)
{
    m_hdlsState[input_idx] = HiddbgHdlsState{};
    m_hdlsState[input_idx].battery_level = 4;

    Result rc = hiddbgSetHdlsState(m_hdlsHandle[input_idx], &m_hdlsState[input_idx]);
    if (R_FAILED(rc))
    {
        syscon::logger::LogError("SwitchMITMHandler[%04x-%04x] The console dropped the device on input: %d right after creating it (Error: 0x%08X)", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, rc);
        ReleaseController(input_idx);
    }

    return rc;
}

/*
    The whole pad goes to hiddbg, Home and Capture included: the real hid publishes it to every
    reader, the mirror copies it into the fake, and am reads the system buttons from it. Only
    on change, since every call is an IPC and a pad at rest reports the same state over and
    over.
*/
Result SwitchMITMHandler::UpdateControllerState(const SwitchPadState &state, uint16_t input_idx)
{
    if (!IsControllerAttached(input_idx))
        return 0;

    HiddbgHdlsState &hdls_state = m_hdlsState[input_idx];
    if (hdls_state.buttons != state.buttons ||
        hdls_state.analog_stick_l.x != state.analog_stick_l.x || hdls_state.analog_stick_l.y != state.analog_stick_l.y ||
        hdls_state.analog_stick_r.x != state.analog_stick_r.x || hdls_state.analog_stick_r.y != state.analog_stick_r.y)
    {
        hdls_state.buttons = state.buttons;
        hdls_state.analog_stick_l = state.analog_stick_l;
        hdls_state.analog_stick_r = state.analog_stick_r;

        Result rc = hiddbgSetHdlsState(m_hdlsHandle[input_idx], &hdls_state);
        if (R_FAILED(rc))
        {
            syscon::logger::LogInfo("SwitchMITMHandler[%04x-%04x] The console dropped the device on input: %d (Error: 0x%08X)", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx, rc);
            ReleaseController(input_idx);
            return 0;
        }
    }

    if (m_controllerList[input_idx] == nullptr)
        return 0;

    return m_controllerList[input_idx]->Update(state);
}

/*
    Only on change: a USB write costs a transfer on the same thread that reads the pad, and a
    game keeps resending the same value every frame for as long as the effect lasts.
*/
Result SwitchMITMHandler::UpdateOutput()
{
    if (!m_controller->Support(SUPPORTS_RUMBLE))
        return 0;

    const uint16_t input_count = std::min<uint16_t>(m_controller->GetInputCount(), CONTROLLER_MAX_INPUTS);
    for (uint16_t input_idx = 0; input_idx < input_count; input_idx++)
    {
        if (!IsControllerAttached(input_idx) || m_controllerList[input_idx] == nullptr)
            continue;

        const controllerlib::RumbleValue rumble = m_controllerList[input_idx]->GetRumble();

        if (rumble == m_lastRumble[input_idx])
            continue;

        m_lastRumble[input_idx] = rumble;

        m_controller->SetRumble(input_idx, rumble);
    }

    return 0;
}
