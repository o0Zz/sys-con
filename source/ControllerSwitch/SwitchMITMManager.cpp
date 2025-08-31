#include "SwitchMITMManager.h"
#include "SwitchLogger.h"
#include <string.h> // memcpy
#include <cinttypes>

#define HID_SHARED_MEMORY_SIZE 0x40000 // 256 KiB
#define POLLING_FREQUENCY_US   10000   // 10ms   // Official software ticks 200 times/second (5 ms per tick)
// #define POLLING_FREQUENCY_US 20000000 // 20s
#define MS_TO_NS(x) (x * 1000000ul)

static HidSharedMemoryManager g_HidSharedMemoryManager;
static HidSharedMemory tmp_shmem_mem;

static_assert(sizeof(HidSharedMemory) == HID_SHARED_MEMORY_SIZE, "HidSharedMemory size is not good!");

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

HidSharedMemoryEntry::HidSharedMemoryEntry(::Service *hid_service, u64 processId)
    : m_process_id(processId)
{
    Handle sharedMemHandle;

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

    ::syscon::logger::LogDebug("HidSharedMemoryEntry created successfully (Process id: 0x%016" PRIx64 ")", m_process_id);
    ::syscon::logger::LogDebug("Fake memory => Handle: %016" PRIx64 ", Size: %zu, Permissions: %u, MapAddr: %p", m_fake_shared_memory.handle, m_fake_shared_memory.size, m_fake_shared_memory.perm, GetFakeAddr());
    ::syscon::logger::LogDebug("Real memory => Handle: %016" PRIx64 ", Size: %zu, Permissions: %u, MapAddr: %p", m_real_shared_memory.handle, m_real_shared_memory.size, m_real_shared_memory.perm, GetRealAddr());
}

HidSharedMemoryEntry::~HidSharedMemoryEntry()
{
    ::syscon::logger::LogDebug("HidSharedMemoryEntry destroyed (Process id: 0x%016" PRIx64 ")", m_process_id);

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

u64 HidSharedMemoryEntry::GetProcessId() const
{
    return m_process_id;
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
    // Do not write any logs in this function, it's a static constructor
    //::syscon::logger::LogDebug("HidSharedMemoryManager::HidSharedMemoryManager created ");
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

int HidSharedMemoryManager::Add(const std::shared_ptr<HidSharedMemoryEntry> &entry)
{
    if (R_FAILED(entry->m_status))
    {
        ::syscon::logger::LogError("HidSharedMemoryManager::Add failed to create HidSharedMemoryEntry: %d", entry->m_status);
        return entry->m_status;
    }

    /*
        Everytime we add a new entry, we run garbage collector
        in order to remove any entries for processes that are no longer running
    */
    RunGarbageCollector();

    ::syscon::logger::LogDebug("HidSharedMemoryManager::Add new entry: %p", entry.get());

    m_mutex.lock();
    m_sharedmemory_entry_list.push_back(entry);
    m_mutex.unlock();

    return 0;
}

void HidSharedMemoryManager::RunGarbageCollector()
{
    ::syscon::logger::LogDebug("HidSharedMemoryManager Run garbage collector: Remove all processes that are no longer running (Total entry: %d)", m_sharedmemory_entry_list.size());

    m_mutex.lock();
    for (auto it = m_sharedmemory_entry_list.begin(); it != m_sharedmemory_entry_list.end();)
    {
        u64 pid_out = 0;

        Result ret = pmdmntGetProcessId(&pid_out, (*it)->GetProcessId());
        if (R_FAILED(ret))
        {
            ::syscon::logger::LogWarning("HidSharedMemoryManager Process id 0x%016" PRIx64 " is not running anymore, remove it ! (Ret: 0x%08X - Mod:%d - Desc:%d)", (*it)->GetProcessId(), ret, R_MODULE(ret), R_DESCRIPTION(ret));
            it = m_sharedmemory_entry_list.erase(it);
        }
        else
        {
            it++;
        }
    }
    m_mutex.unlock();
}

int HidSharedMemoryManager::Start()
{
    if (m_running)
    {
        ::syscon::logger::LogWarning("HidSharedMemoryManager::Start already running, skipping.");
        return 0;
    }

    ::syscon::logger::LogDebug("HidSharedMemoryManager::Start %p starting...", this);

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

        m_mutex.lock();
        for (auto it = m_sharedmemory_entry_list.begin(); it != m_sharedmemory_entry_list.end(); ++it)
        {
            memcpy(&tmp_shmem_mem, (*it)->GetRealAddr(), HID_SHARED_MEMORY_SIZE);
            svcSleepThread(-1);
            memcpy((*it)->GetFakeAddr(), &tmp_shmem_mem, HID_SHARED_MEMORY_SIZE);
        }
        m_mutex.unlock();

        s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();
        if (execution_time_us < POLLING_FREQUENCY_US)
            svcSleepThread((POLLING_FREQUENCY_US - execution_time_us) * 1000); // Convert to nanoseconds
    }
}
