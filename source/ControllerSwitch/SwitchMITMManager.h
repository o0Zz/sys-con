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

    inline ::HidSharedMemory *GetRealAddr();

    inline ::HidSharedMemory *GetFakeAddr();

    inline u64 GetProcessId() const;
    inline u64 GetProgramId() const;

protected:
    void Copy();

private:
    u64 m_process_id;
    u64 m_program_id;
    ::Result m_status;

    ::Service m_appletresource;

    ::SharedMemory m_real_shared_memory;
    ::SharedMemory m_fake_shared_memory;
};

/* ------------------------------------------------ */

class HidSharedMemoryController
{
public:
    HidSharedMemoryController(uint8_t player_idx);
    ~HidSharedMemoryController();

    Result Update(u64 buttons, const HidAnalogStickState &analog_stick_l, const HidAnalogStickState &analog_stick_r);

private:
    uint8_t m_player_idx;
    u64 m_sampling_number;
    HidNpadCommonStateAtomicStorage m_lifo;

    void Initialize(const std::shared_ptr<HidSharedMemoryEntry> &entry);
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

    int Add(const std::shared_ptr<HidSharedMemoryEntry> &entry);

    int Start();
    void Stop();

private:
    void OnRun();

    void RunGarbageCollector();
    void DumpProcessesAndMemoryAddr();
    void DumpHidSharedMemory();

    alignas(0x1000) u8 m_thread_stack[0x4000];

    bool m_running;
    ::Thread m_thread;

protected:
    std::recursive_mutex m_mutex_controller;
    std::array<std::shared_ptr<HidSharedMemoryController>, 8> m_controller_list;

    std::recursive_mutex m_mutex_sharedmemory;
    std::vector<std::shared_ptr<HidSharedMemoryEntry>> m_sharedmemory_entry_list;
};
