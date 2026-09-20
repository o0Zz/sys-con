#pragma once

#include <switch.h>

#include "IController.h"

#include <vector>
#include <memory>
#include <mutex>

/* ------------------------------------------------ */

class HidSharedMemoryEntry
{
    friend class HidSharedMemoryManager;

public:
    HidSharedMemoryEntry(::Service *hid_service, u64 processId, u64 programId);
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

private:
    u64 m_process_id;
    u64 m_program_id;
    ::Result m_status = 0;

    // Zero-initialized on purpose: the constructor gives up at the first failing step, and
    // the destructor still runs. Tearing down an indeterminate Service/SharedMemory closes
    // whatever handle number happened to be on the stack - which on a sysmodule means fs,
    // sm or the MITM port itself, and the console wedges with nothing written to the card.
    ::Service m_appletresource{};

    ::SharedMemory m_real_shared_memory{};
    ::SharedMemory m_fake_shared_memory{};
};

/* ------------------------------------------------ */

class HidSharedMemoryController
{
public:
    HidSharedMemoryController(uint8_t player_idx);
    ~HidSharedMemoryController();

    uint8_t GetPlayerIndex() const { return m_player_idx; }

    // Stores the latest pad state; the manager thread is what writes it into every
    // client's shared memory, at a steady rate the console expects from a real pad.
    Result Update(u64 buttons, const HidAnalogStickState &analog_stick_l, const HidAnalogStickState &analog_stick_r);

    void Publish(HidSharedMemoryEntry &entry);
    void Clear(HidSharedMemoryEntry &entry);

private:
    uint8_t m_player_idx;
    u64 m_sampling_number;

    u64 m_buttons;
    HidAnalogStickState m_analog_stick_l;
    HidAnalogStickState m_analog_stick_r;

    void Initialize(HidNpadInternalState *internal_state);
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

    std::shared_ptr<HidSharedMemoryController> AttachController();
    void DetachController(std::shared_ptr<HidSharedMemoryController> controller);

    std::shared_ptr<HidSharedMemoryEntry> CreateIfNotExists(::Service *hid_service, u64 processId, u64 programId);
    std::shared_ptr<HidSharedMemoryEntry> Get(u64 processId, u64 programId);

    Result Add(const std::shared_ptr<HidSharedMemoryEntry> &entry);

    int Start();
    void Stop();

private:
    void OnRun();

    // real -> fake, for everything but the npad slots sys-con owns.
    void Mirror(HidSharedMemoryEntry &entry);
    bool IsPlayerIndexOwned(uint8_t player_idx) const;
    bool IsPlayerIndexUsedByRealHid(uint8_t player_idx);

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

    std::recursive_mutex m_mutex_sharedmemory;
    std::vector<std::shared_ptr<HidSharedMemoryEntry>> m_sharedmemory_entry_list;
};
