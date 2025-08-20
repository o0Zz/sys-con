#pragma once

#include <switch.h>

#include <unordered_map>
#include <memory>
#include <mutex>

/* ------------------------------------------------ */

class HidSharedMemoryEntry
{
    friend class HidSharedMemoryManager;

public:
    HidSharedMemoryEntry(::Service *hid_service);
    ~HidSharedMemoryEntry();

    const ::SharedMemory &GetSharedMemoryHandle() const;
    inline void *GetRealAddr();
    inline void *GetFakeAddr();

protected:
    void Copy();

private:
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

    int Add(u64 pid, const std::shared_ptr<HidSharedMemoryEntry> &entry);
    void Remove(u64 pid);

    int Start();
    void Stop();

private:
    void OnRun();

    alignas(0x1000) u8 m_thread_stack[0x4000];

    bool m_running;
    Thread m_thread;
    std::recursive_mutex m_mutex;

    std::unordered_map<u64, std::shared_ptr<HidSharedMemoryEntry>> m_sharedmemory_entry_list;
};
