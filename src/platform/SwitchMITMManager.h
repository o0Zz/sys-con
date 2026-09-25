#pragma once

#include <switch.h>

#include "IController.h"
#include "SwitchMotion.h"
#include "SwitchPadState.h"

#include <array>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

/* ------------------------------------------------ */

enum class HidFakeView : u8
{
    System,
    Application,
};

constexpr size_t HidFakeViewCount = 2;

/* ------------------------------------------------ */

class HidSharedMemoryEntry
{
    friend class HidSharedMemoryManager;

public:
    HidSharedMemoryEntry(::Service *hid_service, u64 aruid, u64 processId, u64 programId);
    ~HidSharedMemoryEntry();

    const ::SharedMemory &GetSharedMemoryHandle() const;

    // Real (forwarded) IAppletResource service, so a hand-written MITM server can forward
    // any non-hooked IAppletResource command straight to the original object.
    const ::Service *GetForwardAppletResource() const { return &m_appletresource; }

    // Forget the forwarded IAppletResource without closing it. A MITM server calls this when
    // the domain the object lives in is going away: the destructor would otherwise send a
    // close request down a session handle the kernel has already handed to somebody else.
    void AbandonForwardAppletResource() { m_appletresource = ::Service{}; }

    inline ::HidSharedMemory *GetRealAddr();

    inline ::HidSharedMemory *GetFakeAddr();

    inline u64 GetProcessId() const;
    inline u64 GetProgramId() const;
    HidFakeView GetView() const { return m_view; }

private:
    u64 m_process_id;
    u64 m_program_id;
    HidFakeView m_view;
    ::Result m_status = 0;

    // Zero-initialized on purpose: the constructor gives up at the first failing step, and
    // the destructor still runs. Tearing down an indeterminate Service/SharedMemory closes
    // whatever handle number happened to be on the stack - which on a sysmodule means fs,
    // sm or the MITM port itself, and the console wedges with nothing written to the card.
    ::Service m_appletresource{};

    ::SharedMemory m_real_shared_memory{};
};

/* ------------------------------------------------ */

class HidSharedMemoryController
{
public:
    static constexpr uint8_t VibrationDeviceCount = 2;

    // device_type is the HidDeviceType the pad's hiddbg device was created with; it decides
    // what kind of controller the fake shared memory presents.
    HidSharedMemoryController(uint8_t player_idx, u8 device_type, u32 body_color, u32 buttons_color);

    uint8_t GetPlayerIndex() const { return m_player_idx; }

    // Stores the latest pad state; the manager thread is what writes it into every
    // client's shared memory, at a steady rate the console expects from a real pad.
    Result Update(const SwitchPadState &state);

    // Vibration flows the other way: a mitm'd hid command stores it in the manager and the
    // handler's polling thread drains it into the driver.
    controllerlib::RumbleValue GetRumble() const;

    void Publish();
    void Clear();

    struct NpadIdentity
    {
        u32 style;
        u32 device_type_bits;
        u8 footer;
    };

private:
    uint8_t m_player_idx;
    u8 m_device_type;
    u32 m_body_color;
    u32 m_buttons_color;
    u64 m_sampling_number;

    SwitchPadState m_state;
    SwitchMotion m_motion;

    NpadIdentity IdentityFor(HidFakeView view) const;
    void Initialize(HidNpadInternalState *internal_state, const NpadIdentity &identity);
    static void ApplyIdentity(HidNpadInternalState *internal_state, const NpadIdentity &identity);
};

/* ------------------------------------------------ */

class HidSharedMemoryManager
{
    friend void HidSharedMemoryManagerThreadFunc(void *arg);
    friend HidSharedMemoryController;

public:
    HidSharedMemoryManager();
    ~HidSharedMemoryManager();

    static HidSharedMemoryManager &GetHidSharedMemoryManager();

    // The slot is dictated by the caller, not chosen here: the pad has already been created
    // through hiddbg and the console has given it an npad, and it is that npad the fake
    // shared memory has to override.
    std::shared_ptr<HidSharedMemoryController> AttachControllerAt(uint8_t player_idx, u8 device_type, u32 body_color, u32 buttons_color);

    /*
        hid only shows a client the npad styles it declared through SetSupportedNpadStyleSet,
        and a game handed a style it never asked for asserts. The application's declaration is
        kept so its view can present a GameCube or N64 pad as one exactly when the game
        supports it; every system applet shares a view and is shown a Pro Controller.
    */
    void OnSupportedNpadStyleSet(u64 program_id, u32 style_set);
    u32 GetApplicationSupportedStyles() const { return m_application_styles.load(std::memory_order_relaxed); }
    void DetachController(std::shared_ptr<HidSharedMemoryController> controller);

    /*
        The vibration side of the manager takes no lock, and nothing below may be made to.
        It is called from the MITM server thread, which runs at priority 20 while the pad
        threads run at 41 and the mirror thread at 38 - all three pinned to CPU 3. Blocking
        the MITM thread behind a pad thread that the mirror thread keeps preempting stalls
        every hid client on the console, which takes am, hid and sm down with it.
    */
    bool IsPlayerIndexOwned(uint8_t player_idx) const;

    /*
        A client disconnecting one of our npads through hid - the grip/order screen does it to
        every npad the moment it opens. The slot is cleared here, before the request is even
        forwarded, so the client can never read our pad back after its own disconnect; the pad
        handler then sees IsPlayerIndexRetired and releases its hiddbg device on its own thread.
    */
    void RetireDisconnectedNpad(u32 npad_id);
    bool IsPlayerIndexRetired(uint8_t player_idx) const;

    void SetVibration(uint8_t player_idx, uint8_t device_idx, const HidVibrationValue &value);
    HidVibrationValue GetVibration(uint8_t player_idx, uint8_t device_idx) const;
    controllerlib::RumbleValue GetRumble(uint8_t player_idx) const;
    void ClearVibration(uint8_t player_idx);

    std::shared_ptr<HidSharedMemoryEntry> CreateIfNotExists(::Service *hid_service, u64 aruid, u64 processId, u64 programId);

    Result Add(const std::shared_ptr<HidSharedMemoryEntry> &entry);

    int Start();
    void Stop();

private:
    void OnRun();

    // real -> fake, for everything but the npad slots sys-con owns.
    void Mirror(HidSharedMemoryEntry &entry);

    // Hands an npad slot back to the real hid in every view. Caller holds m_mutex_sharedmemory.
    void RestoreRealSlot(uint8_t player_idx);
    std::shared_ptr<HidSharedMemoryEntry> FindEntry(HidFakeView view) const;

    void RunGarbageCollector();
    void DumpProcessesAndMemoryAddr();

    alignas(0x1000) u8 m_thread_stack[0x4000];

    bool m_running;
    ::Thread m_thread;

protected:
    // Lock order is always m_mutex_controller then m_mutex_sharedmemory: attaching a
    // controller has to reach into every client's shared memory to clear its slot.
    std::recursive_mutex m_mutex_controller;
    std::array<std::shared_ptr<HidSharedMemoryController>, 8> m_controller_list;

    // Read without any lock; see the note on IsPlayerIndexOwned above. Individual fields can
    // be read while another is being written, which for a motor amplitude is harmless.
    struct VibrationSlot
    {
        std::atomic<float> amp_low{0.0f};
        std::atomic<float> amp_high{0.0f};
        std::atomic<float> freq_low{controllerlib::RumbleActuator{}.freq_low};
        std::atomic<float> freq_high{controllerlib::RumbleActuator{}.freq_high};
    };

    std::array<std::atomic<bool>, 8> m_player_owned;
    std::array<std::atomic<bool>, 8> m_player_retired;
    std::atomic<u32> m_application_styles{0};
    std::array<VibrationSlot, 8 * HidSharedMemoryController::VibrationDeviceCount> m_vibration;

    std::recursive_mutex m_mutex_sharedmemory;
    std::vector<std::shared_ptr<HidSharedMemoryEntry>> m_sharedmemory_entry_list;
};
