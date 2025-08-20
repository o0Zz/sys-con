#include "SwitchMITMManager.h"
#include "SwitchLogger.h"
#include <string.h> // memcpy
#include <cinttypes>

#define HID_SHARED_MEMORY_SIZE 0x40000 // 256 KiB
#define POLLING_FREQUENCY_US   10000   // 10ms   // Official software ticks 200 times/second (5 ms per tick)
#define MS_TO_NS(x)            (x * 1000000ul)

static HidSharedMemoryManager g_HidSharedMemoryManager;

/************************************************************
              HidSharedMemoryEntry
************************************************************/

static Result _HidCreateAppletResource(Service *srv, Service *out_iappletresource)
{
    u64 AppletResourceUserId = appletGetAppletResourceUserId();

    return serviceDispatchIn(srv, 0, AppletResourceUserId,
                             .in_send_pid = true,
                             .out_num_objects = 1,
                             .out_objects = out_iappletresource, );
}

static Result _HidGetSharedMemoryHandle(Service *srv, Handle *handle_out)
{
    return serviceDispatch(srv, 0,
                           .out_handle_attrs = {SfOutHandleAttr_HipcCopy},
                           .out_handles = handle_out, );
}

HidSharedMemoryEntry::HidSharedMemoryEntry(::Service *hid_service)
{
    Handle sharedMemHandle;

    ::syscon::logger::LogDebug("HidSharedMemoryEntry::HidSharedMemoryEntry creating ...");

    m_status = _HidCreateAppletResource(hid_service, &m_appletresource); // Executes the original ipc
    if (R_FAILED(m_status))
        return;

    m_status = _HidGetSharedMemoryHandle(&m_appletresource, &sharedMemHandle);
    if (R_FAILED(m_status))
        return;

    shmemLoadRemote(&m_real_shared_memory, sharedMemHandle, HID_SHARED_MEMORY_SIZE, Perm_R);
    m_status = shmemMap(&m_real_shared_memory);
    if (R_FAILED(m_status))
        return;

    shmemCreate(&m_fake_shared_memory, HID_SHARED_MEMORY_SIZE, Perm_Rw, Perm_R); // sizeof(HidSharedMemory)
    m_status = shmemMap(&m_fake_shared_memory);
    if (R_FAILED(m_status))
        return;

    ::syscon::logger::LogDebug("HidSharedMemoryEntry::HidSharedMemoryEntry created !");

    ::syscon::logger::LogDebug("Fake memory => Handle: %016" PRIx64 ", Size: %zu, Permissions: %u, MapAddr: %p", m_fake_shared_memory.handle, m_fake_shared_memory.size, m_fake_shared_memory.perm, GetFakeAddr());
    ::syscon::logger::LogDebug("Real memory => Handle: %016" PRIx64 ", Size: %zu, Permissions: %u, MapAddr: %p", m_real_shared_memory.handle, m_real_shared_memory.size, m_real_shared_memory.perm, GetRealAddr());
}

HidSharedMemoryEntry::~HidSharedMemoryEntry()
{
    shmemUnmap(&m_real_shared_memory);
    shmemClose(&m_real_shared_memory);

    shmemUnmap(&m_fake_shared_memory);
    shmemClose(&m_fake_shared_memory);

    serviceClose(&m_appletresource);
}

const ::SharedMemory &HidSharedMemoryEntry::GetSharedMemoryHandle() const
{
    return m_fake_shared_memory;
}

void *HidSharedMemoryEntry::GetRealAddr()
{
    return (HidSharedMemory *)shmemGetAddr(&m_real_shared_memory);
}

void *HidSharedMemoryEntry::GetFakeAddr()
{
    return (HidSharedMemory *)shmemGetAddr(&m_fake_shared_memory);
}

/************************************************************
              HidSharedMemoryManager
************************************************************/

void HidSharedMemoryManagerThreadFunc(void *manager)
{
    static_cast<HidSharedMemoryManager *>(manager)->OnRun();
}

HidSharedMemoryManager::HidSharedMemoryManager()
    : m_running(false)
{
    ::syscon::logger::LogDebug("HidSharedMemoryManager::HidSharedMemoryManager created ");
}

HidSharedMemoryManager::~HidSharedMemoryManager()
{
    Stop();
    ::syscon::logger::LogDebug("HidSharedMemoryManager::~HidSharedMemoryManager destroyed.");
}

HidSharedMemoryManager &HidSharedMemoryManager::GetHidSharedMemoryManager()
{
    return g_HidSharedMemoryManager;
}

int HidSharedMemoryManager::Add(u64 pid, const std::shared_ptr<HidSharedMemoryEntry> &entry)
{
    if (R_FAILED(entry->m_status))
    {
        ::syscon::logger::LogError("HidSharedMemoryManager::Add failed to create HidSharedMemoryEntry: %d", entry->m_status);
        return entry->m_status;
    }

    ::syscon::logger::LogDebug("HidSharedMemoryManager::Add PID: 0x%016" PRIx64 ", Entry: %p", pid, entry.get());

    m_mutex.lock();
    m_sharedmemory_entry_list[pid] = entry;
    m_mutex.unlock();

    return 0;
}

void HidSharedMemoryManager::Remove(u64 pid)
{
    m_mutex.lock();
    m_sharedmemory_entry_list.erase(pid);
    m_mutex.unlock();
}

int HidSharedMemoryManager::Start()
{
    if (m_running)
    {
        ::syscon::logger::LogWarning("HidSharedMemoryManager::Start already running, skipping.");
        return 0;
    }

    ::syscon::logger::LogDebug("HidSharedMemoryManager::Start starting...");

    m_running = true;
    Result rc = threadCreate(&m_thread, &HidSharedMemoryManagerThreadFunc, this, m_thread_stack, sizeof(m_thread_stack), 41, -2);
    if (R_FAILED(rc))
        return rc;

    rc = threadStart(&m_thread);
    if (R_FAILED(rc))
        return rc;

    return 0;
}

void HidSharedMemoryManager::Stop()
{
    if (!m_running)
        return;

    ::syscon::logger::LogDebug("HidSharedMemoryManager::Stop stopping...");
    m_running = false;

    svcCancelSynchronization(m_thread.handle);

    threadWaitForExit(&m_thread);
    threadClose(&m_thread);
}

void HidSharedMemoryManager::OnRun()
{
    ::syscon::logger::LogDebug("HidSharedMemoryManager::OnRun started...");

    while (m_running)
    {
        auto startTimer = std::chrono::steady_clock::now();
        HidSharedMemory tmp_shmem_mem;

        m_mutex.lock();
        for (auto it = m_sharedmemory_entry_list.begin(); it != m_sharedmemory_entry_list.end();)
        {
            ::syscon::logger::LogDebug("HidSharedMemoryManager::OnRun Copying memory for PID: 0x%016" PRIx64 "", it->first);

            memcpy(it->second->GetRealAddr(), &tmp_shmem_mem, HID_SHARED_MEMORY_SIZE);
            svcSleepThread(-1);
            memcpy(&tmp_shmem_mem, it->second->GetFakeAddr(), HID_SHARED_MEMORY_SIZE);
            it++;
        }
        m_mutex.unlock();

        s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();
        if (execution_time_us < POLLING_FREQUENCY_US)
            svcSleepThread((POLLING_FREQUENCY_US - execution_time_us) * 1000); // Convert to nanoseconds
    }
}
