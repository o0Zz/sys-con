#include "SwitchMITMManager.h"
#include "SwitchLogger.h"
#include <string.h> // memcpy
#include <cinttypes>

#define HID_SHARED_MEMORY_SIZE 0x40000 // 256 KiB
#define POLLING_FREQUENCY_US   10000   // 10ms   // Official software ticks 200 times/second (5 ms per tick)
// #define POLLING_FREQUENCY_US 5000000 // 5s
#define MS_TO_NS(x) (x * 1000000ul)

static HidSharedMemoryManager g_HidSharedMemoryManager;
static HidSharedMemory tmp_shmem_mem;
static HidSharedMemory tmp_shmem_mem_dmp;

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

HidSharedMemoryEntry::HidSharedMemoryEntry(::Service *hid_service, u64 processId, u64 programId)
    : m_process_id(processId), m_program_id(programId)
{
    Handle sharedMemHandle;

    m_status = _HidCreateAppletResource(hid_service, &m_appletresource); // Executes the original ipc
    if (R_FAILED(m_status))
    {
        ::syscon::logger::LogError("HidSharedMemoryEntry failed to create applet resource (Process id: 0x%016" PRIx64 ")", m_process_id);
        return;
    }

    m_status = _HidGetSharedMemoryHandle(&m_appletresource, &sharedMemHandle);
    if (R_FAILED(m_status))
    {
        ::syscon::logger::LogError("HidSharedMemoryEntry failed to get shared memory handle (Process id: 0x%016" PRIx64 ")", m_process_id);
        return;
    }

    shmemLoadRemote(&m_real_shared_memory, sharedMemHandle, HID_SHARED_MEMORY_SIZE, Perm_R);
    m_status = shmemMap(&m_real_shared_memory);
    if (R_FAILED(m_status))
    {
        ::syscon::logger::LogError("HidSharedMemoryEntry failed to map real shared memory (MITM ...Process id: 0x%016" PRIx64 ")", m_process_id);
        return;
    }

    shmemCreate(&m_fake_shared_memory, HID_SHARED_MEMORY_SIZE, Perm_Rw, Perm_R); // sizeof(HidSharedMemory)
    m_status = shmemMap(&m_fake_shared_memory);
    if (R_FAILED(m_status))
    {
        ::syscon::logger::LogError("HidSharedMemoryEntry failed to map fake shared memory (Process id: 0x%016" PRIx64 ")", m_process_id);
        return;
    }

    ::syscon::logger::LogDebug("HidSharedMemoryEntry created successfully (Process id: 0x%016" PRIx64 ", RealAddr: %p, FakeAddr: %p)", m_process_id, GetRealAddr(), GetFakeAddr());
    //::syscon::logger::LogDebug("Fake memory => Handle: %016" PRIx64 ", Size: %zu, Permissions: %u, MapAddr: %p", m_fake_shared_memory.handle, m_fake_shared_memory.size, m_fake_shared_memory.perm, GetFakeAddr());
    //::syscon::logger::LogDebug("Real memory => Handle: %016" PRIx64 ", Size: %zu, Permissions: %u, MapAddr: %p", m_real_shared_memory.handle, m_real_shared_memory.size, m_real_shared_memory.perm, GetRealAddr());
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

::HidSharedMemory *HidSharedMemoryEntry::GetRealAddr()
{
    return (HidSharedMemory *)shmemGetAddr(&m_real_shared_memory);
}

::HidSharedMemory *HidSharedMemoryEntry::GetFakeAddr()
{
    return (HidSharedMemory *)shmemGetAddr(&m_fake_shared_memory);
}

u64 HidSharedMemoryEntry::GetProcessId() const
{
    return m_process_id;
}

u64 HidSharedMemoryEntry::GetProgramId() const
{
    return m_program_id;
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

std::shared_ptr<HidSharedMemoryEntry> HidSharedMemoryManager::CreateIfNotExists(::Service *hid_service, u64 processId, u64 programId)
{
    std::shared_ptr<HidSharedMemoryEntry> entry = Get(processId, programId);
    if (entry != nullptr)
    {
        ::syscon::logger::LogDebug("HidSharedMemoryManager::CreateIfNotExists entry already exists (Process id: 0x%016" PRIx64 ", Program id: 0x%016" PRIx64 ")", processId, programId);
        return entry;
    }

    entry = std::make_shared<HidSharedMemoryEntry>(hid_service, processId, programId);
    Add(entry);

    return entry;
}

int HidSharedMemoryManager::Add(const std::shared_ptr<HidSharedMemoryEntry> &entry)
{
    ::syscon::logger::LogDebug("HidSharedMemoryManager Adding entry: %p for process id: 0x%016" PRIx64, entry.get(), entry->GetProcessId());

    if (R_FAILED(entry->m_status))
    {
        ::syscon::logger::LogError("HidSharedMemoryManager::Add failed to create HidSharedMemoryEntry: %d", entry->m_status);
        return entry->m_status;
    }

    /*
        Everytime we add a new entry, we run garbage collector
        in order to remove any entries for processes that are no longer running
    */
    // RunGarbageCollector();

    m_mutex.lock();
    m_sharedmemory_entry_list.push_back(entry);
    m_mutex.unlock();

    DumpProcessesAndMemoryAddr();

    return 0;
}

std::shared_ptr<HidSharedMemoryEntry> HidSharedMemoryManager::Get(u64 processId, u64 programId)
{
    m_mutex.lock();
    for (auto it = m_sharedmemory_entry_list.begin(); it != m_sharedmemory_entry_list.end(); it++)
    {
        if (processId == (*it)->GetProcessId() && programId == (*it)->GetProgramId())
        {
            m_mutex.unlock();
            return *it;
        }
    }
    m_mutex.unlock();

    return nullptr;
}

void HidSharedMemoryManager::RunGarbageCollector()
{
    m_mutex.lock();
    for (auto it = m_sharedmemory_entry_list.begin(); it != m_sharedmemory_entry_list.end();)
    {
        u64 pid_out = 0;

        Result ret = pmdmntGetProcessId(&pid_out, (*it)->GetProgramId());
        if (R_SUCCEEDED(ret) && pid_out == (*it)->GetProcessId())
        {
            it++;
            continue;
        }

        ::syscon::logger::LogWarning("HidSharedMemoryManager Process id 0x%016" PRIx64 " is not running anymore, remove it ! (Ret: 0x%08X - Mod:%d - Desc:%d)", (*it)->GetProcessId(), ret, R_MODULE(ret), R_DESCRIPTION(ret));
        it = m_sharedmemory_entry_list.erase(it);
    }
    m_mutex.unlock();
}

void HidSharedMemoryManager::DumpProcessesAndMemoryAddr()
{
    ::syscon::logger::LogDebug("_____________________________________________________________________________________");
    ::syscon::logger::LogDebug("|     Program ID     |     Process ID     |      FakeAddr      |      RealAddr      |");
    m_mutex.lock();
    for (const auto &entry : m_sharedmemory_entry_list)
    {
        ::syscon::logger::LogDebug("| 0x%016" PRIx64 " | 0x%016" PRIx64 " | 0x%016" PRIx64 " | 0x%016" PRIx64 " |",
                                   entry->GetProgramId(), entry->GetProcessId(), entry->GetFakeAddr(), entry->GetRealAddr());
    }
    m_mutex.unlock();
    ::syscon::logger::LogDebug("_____________________________________________________________________________________");
}
#include <stratosphere.hpp>
void HidSharedMemoryManager::DumpHidSharedMemory()
{
    ams::fs::FileHandle file;

    ::syscon::logger::LogDebug("Dumping HID shared memory...");

    m_mutex.lock();
    for (auto it = m_sharedmemory_entry_list.begin(); it != m_sharedmemory_entry_list.end(); ++it)
    {
        std::string filename = "sdmc:/config/sys-con/HidMemory_" + std::to_string((*it)->GetProgramId()) + ".dmp";
        memcpy(&tmp_shmem_mem_dmp, (*it)->GetRealAddr(), sizeof(HidSharedMemory));

        ::syscon::logger::LogDebug("Opening %s...", filename.c_str());

        ams::fs::DeleteFile(filename.c_str());
        ams::fs::CreateFile(filename.c_str(), 0);
        ams::fs::OpenFile(std::addressof(file), filename.c_str(), ams::fs::OpenMode_Write | ams::fs::OpenMode_AllowAppend);
        ams::fs::WriteFile(file, 0, &tmp_shmem_mem_dmp, sizeof(HidSharedMemory), ams::fs::WriteOption::Flush);
        ams::fs::CloseFile(file);
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
    ::syscon::logger::LogDebug("HidSharedMemoryManager::OnRun running...");

    while (m_running)
    {
        auto startTimer = std::chrono::steady_clock::now();

        static u64 loop_count = 0;
        if (loop_count++ == 1000) // 10s after boot
            DumpHidSharedMemory();

        m_mutex.lock();

        memset(&tmp_shmem_mem, 0, sizeof(tmp_shmem_mem));

        for (auto it = m_sharedmemory_entry_list.begin(); it != m_sharedmemory_entry_list.end(); ++it)
        {
            // memcpy(&tmp_shmem_mem, (*it)->GetRealAddr(), HID_SHARED_MEMORY_SIZE);
            // memcpy((*it)->GetFakeAddr(), &tmp_shmem_mem, HID_SHARED_MEMORY_SIZE);

            // memcpy(&(*it)->GetFakeAddr()->touchscreen, &(*it)->GetRealAddr()->touchscreen, sizeof(HidTouchScreenSharedMemoryFormat));
            //  Apply diff here
            /*for (int i = 0; i < 10; i++)
            {
                HidNpadInternalState *internal_state = &tmp_shmem_mem.npad.entries[i].internal_state;
                if (internal_state->style_set == 0)
                {
                    internal_state->style_set = HidNpadStyleTag_NpadFullKey;
                    internal_state->joy_assignment_mode = HidNpadJoyAssignmentMode_Dual;
                    internal_state->full_key_color.attribute = HidColorAttribute_Ok;
                    internal_state->full_key_color.full_key.main = 0xFF0000FF; // BodyColor
                    internal_state->full_key_color.full_key.sub = 0xFF0000FF;  // ButtonColor
                    internal_state->full_key_lifo.header.unused = 0;
                    internal_state->full_key_lifo.header.buffer_count = 1;
                    internal_state->full_key_lifo.header.tail = 0;
                    internal_state->full_key_lifo.header.count = 0;
                    internal_state->full_key_lifo.storage[0].sampling_number = 0;
                    internal_state->full_key_lifo.storage[0].state.sampling_number = 0;
                    internal_state->full_key_lifo.storage[0].state.buttons = 0; // Bitfield
                    internal_state->full_key_lifo.storage[0].state.analog_stick_l.x = 0;
                    internal_state->full_key_lifo.storage[0].state.analog_stick_l.y = 0;
                    internal_state->full_key_lifo.storage[0].state.analog_stick_r.x = 0;
                    internal_state->full_key_lifo.storage[0].state.analog_stick_r.y = 0;
                    internal_state->full_key_lifo.storage[0].state.attributes = HidNpadAttribute_IsConnected | HidNpadAttribute_IsWired;

                    internal_state->device_type = HidDeviceTypeBits_FullKey;
                    internal_state->system_properties.is_charging = 0;
                    internal_state->system_properties.is_powered = 1;
                    internal_state->system_properties.is_unsupported_button_pressed_on_npad_system = 0;
                    internal_state->system_properties.is_unsupported_button_pressed_on_npad_system_ext = 0;
                    internal_state->system_properties.is_abxy_button_oriented = 0;
                    internal_state->system_properties.is_sl_sr_button_oriented = 0;
                    internal_state->system_properties.is_plus_available = 1;
                    internal_state->system_properties.is_minus_available = 1;
                    internal_state->system_properties.is_directional_buttons_available = 1;
                    internal_state->system_button_properties.is_unintended_home_button_input_protection_enabled = 0;
                    internal_state->battery_level[0] = 100;

                    break;
                }
            }*/
        }
        m_mutex.unlock();

        /*for (int i = 0; i < 10; i++)
        {
            HidNpadInternalState *internal_state = &tmp_shmem_mem.npad.entries[i].internal_state;
            ::syscon::logger::LogDebug("HidSharedMemoryManager::OnRun internal_state->style_set[%d]: 0x%02X", i, internal_state->style_set);
        }*/

        s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();
        if (execution_time_us < POLLING_FREQUENCY_US)
            svcSleepThread((POLLING_FREQUENCY_US - execution_time_us) * 1000); // Convert to nanoseconds
    }
}
