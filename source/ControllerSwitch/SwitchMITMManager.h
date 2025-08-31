#pragma once

#include <switch.h>

#include <vector>
#include <memory>
#include <mutex>

/* ------------------------------------------------ */

class HidSharedMemoryEntry
{
    friend class HidSharedMemoryManager;

public:
    HidSharedMemoryEntry(::Service *hid_service, u64 processId);
    ~HidSharedMemoryEntry();

    const ::SharedMemory &GetSharedMemoryHandle() const;

    inline void *GetRealAddr();

    inline void *GetFakeAddr();

    inline u64 GetProcessId() const;

protected:
    void Copy();

private:
    u64 m_process_id;
    ::Result m_status;

    ::Service m_appletresource;

    ::SharedMemory m_real_shared_memory;
    ::SharedMemory m_fake_shared_memory;
};

/* ------------------------------------------------ */

class HidSharedMemoryManager
{
    friend void HidSharedMemoryManagerThreadFunc(void *arg);

public:
    HidSharedMemoryManager();
    ~HidSharedMemoryManager();

    static HidSharedMemoryManager &GetHidSharedMemoryManager();

    int Add(const std::shared_ptr<HidSharedMemoryEntry> &entry);

    int Start();
    void Stop();

private:
    void OnRun();

    void RunGarbageCollector();

    alignas(0x1000) u8 m_thread_stack[0x4000];

    bool m_running;
    ::Thread m_thread;
    std::recursive_mutex m_mutex;

    std::vector<std::shared_ptr<HidSharedMemoryEntry>> m_sharedmemory_entry_list;
};
