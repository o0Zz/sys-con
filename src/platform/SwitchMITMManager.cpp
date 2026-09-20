#include "SwitchMITMManager.h"
#include "SwitchLogger.h"
#include <string.h> // memcpy
#include <cinttypes>
#include <atomic>
#include <chrono>

#define HID_SHARED_MEMORY_SIZE 0x40000 // 256 KiB
#define POLLING_FREQUENCY_US   5000    // 5ms // Official software ticks 200 times/second

static HidSharedMemoryManager g_HidSharedMemoryManager;

#define MITM_CONFIG_REUSE_SHARED_MEMORY 0 // Set to 1 to reuse the shared memory on maximum
#define MITM_CONFIG_GC_ENABLED          0 // Set to 1 to enable garbage collection for the shared memory

static_assert(sizeof(HidSharedMemory) == HID_SHARED_MEMORY_SIZE, "HidSharedMemory size is not good!");

namespace
{
    constexpr size_t NpadOffset = offsetof(HidSharedMemory, npad);
    constexpr size_t NpadSize = sizeof(HidNpadSharedMemoryFormat);
    constexpr size_t NpadEntryCount = sizeof(HidNpadSharedMemoryFormat) / sizeof(HidNpadSharedMemoryEntry);
    constexpr size_t AfterNpadOffset = NpadOffset + NpadSize;
    // Everything past console_six_axis_sensor is unused padding, so it is never mirrored.
    constexpr size_t AfterNpadSize = offsetof(HidSharedMemory, unk_x3C220) - AfterNpadOffset;

    u8 *ByteAddr(HidSharedMemory *shmem, size_t offset)
    {
        return reinterpret_cast<u8 *>(shmem) + offset;
    }
} // namespace

static void memcpy_64(void *dest, const void *src, size_t n)
{
    if (n % 8 != 0 || reinterpret_cast<uintptr_t>(dest) % 8 != 0 || reinterpret_cast<uintptr_t>(src) % 8 != 0)
    {
        ::syscon::logger::LogWarning("memcpy_64: n is not a multiple of 8 (%zu) or address is not 8-byte aligned (dest: %p, src: %p)", n, dest, src);
        const uint8_t *s = static_cast<const uint8_t *>(src);
        uint8_t *d = static_cast<uint8_t *>(dest);
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    }
    else
    {
        const volatile uint64_t *s = reinterpret_cast<const volatile uint64_t *>(src);
        volatile uint64_t *d = reinterpret_cast<volatile uint64_t *>(dest);
        for (size_t i = 0; i < (n / 8); i++)
            d[i] = s[i];
    }
}

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

    ::syscon::logger::LogInfo("HidSharedMemoryEntry created successfully (Process id: 0x%016" PRIx64 ", RealAddr: %p, FakeAddr: %p)", m_process_id, GetRealAddr(), GetFakeAddr());

    // Initialize the fake shared memory with the content of the real shared memory
    memcpy_64(GetFakeAddr(), GetRealAddr(), HID_SHARED_MEMORY_SIZE);
}

static void CloseSharedMemory(::SharedMemory *shared_memory)
{
    if (shared_memory->map_addr != nullptr)
        shmemUnmap(shared_memory);

    if (shared_memory->handle != INVALID_HANDLE)
        shmemClose(shared_memory);
}

HidSharedMemoryEntry::~HidSharedMemoryEntry()
{
    ::syscon::logger::LogDebug("HidSharedMemoryEntry destroyed (Process id: 0x%016" PRIx64 ")", m_process_id);

    CloseSharedMemory(&m_real_shared_memory);
    CloseSharedMemory(&m_fake_shared_memory);

    if (serviceIsActive(&m_appletresource))
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

bool HidSharedMemoryManager::IsPlayerIndexOwned(uint8_t player_idx) const
{
    return m_controller_list[player_idx] != nullptr;
}

bool HidSharedMemoryManager::IsPlayerIndexUsedByRealHid(uint8_t player_idx)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex_sharedmemory);

    for (const auto &entry : m_sharedmemory_entry_list)
    {
        if (entry->GetRealAddr()->npad.entries[player_idx].internal_state.style_set != 0)
            return true;
    }

    return false;
}

std::shared_ptr<HidSharedMemoryController> HidSharedMemoryManager::AttachController()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex_controller);

    for (uint8_t i = 0; i < m_controller_list.size(); i++)
    {
        if (IsPlayerIndexOwned(i) || IsPlayerIndexUsedByRealHid(i))
            continue;

        m_controller_list[i] = std::make_shared<HidSharedMemoryController>(i);

        std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);
        for (const auto &entry : m_sharedmemory_entry_list)
            m_controller_list[i]->Clear(*entry);

        ::syscon::logger::LogInfo("HidSharedMemoryManager attached a controller on player %d", i + 1);
        return m_controller_list[i];
    }

    ::syscon::logger::LogError("HidSharedMemoryManager has no free player slot left, controller not attached !");
    return nullptr;
}

void HidSharedMemoryManager::DetachController(std::shared_ptr<HidSharedMemoryController> controller)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex_controller);

    for (size_t i = 0; i < m_controller_list.size(); i++)
    {
        if (m_controller_list[i] != controller)
            continue;

        std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);
        for (const auto &entry : m_sharedmemory_entry_list)
            controller->Clear(*entry);

        ::syscon::logger::LogInfo("HidSharedMemoryManager detached the controller of player %d", (int)i + 1);
        m_controller_list[i] = nullptr;
        return;
    }
}

std::shared_ptr<HidSharedMemoryEntry> HidSharedMemoryManager::CreateIfNotExists(::Service *hid_service, u64 processId, u64 programId)
{
    std::shared_ptr<HidSharedMemoryEntry> entry;

#if MITM_CONFIG_REUSE_SHARED_MEMORY
    entry = Get(processId, programId);
    if (entry != nullptr)
    {
        ::syscon::logger::LogDebug("HidSharedMemoryManager::CreateIfNotExists entry already exists (Process id: 0x%016" PRIx64 ", Program id: 0x%016" PRIx64 ")", processId, programId);
        return entry;
    }
#endif

    entry = std::make_shared<HidSharedMemoryEntry>(hid_service, processId, programId);

    // A half-built entry must never reach the client: its fake shared memory handle is
    // invalid, and handing that out as a copy handle is worse than failing the command.
    if (R_FAILED(Add(entry)))
        return nullptr;

    return entry;
}

Result HidSharedMemoryManager::Add(const std::shared_ptr<HidSharedMemoryEntry> &entry)
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
#if MITM_CONFIG_GC_ENABLED
    RunGarbageCollector();
#endif

    std::lock_guard<std::recursive_mutex> lock(m_mutex_controller);

    /*
        The entry starts life as a byte copy of the real shared memory, so the slots
        sys-con drives still hold whatever the console had there. Clearing them is what
        makes Publish() see style_set == 0 and lay the virtual npad out from scratch.
    */
    for (const auto &controller : m_controller_list)
    {
        if (controller != nullptr)
            controller->Clear(*entry);
    }

    m_mutex_sharedmemory.lock();
    m_sharedmemory_entry_list.push_back(entry);
    m_mutex_sharedmemory.unlock();

    DumpProcessesAndMemoryAddr();

    return 0;
}

std::shared_ptr<HidSharedMemoryEntry> HidSharedMemoryManager::Get(u64 processId, u64 programId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex_sharedmemory);

    for (const auto &entry : m_sharedmemory_entry_list)
    {
        if (processId == entry->GetProcessId() && programId == entry->GetProgramId())
            return entry;
    }

    return nullptr;
}

void HidSharedMemoryManager::RunGarbageCollector()
{
    ::syscon::logger::LogDebug("HidSharedMemoryManager Garbage Collector running...");

    // pm:dmnt accepts a single session, so hold it only for this sweep rather
    // than for the process lifetime: anything else on the console that opens
    // it - including whatever launched us - fails with SessionClosed while we
    // keep it.
    if (R_FAILED(pmdmntInitialize()))
    {
        ::syscon::logger::LogWarning("HidSharedMemoryManager: pm:dmnt unavailable, skipping garbage collection");
        return;
    }

    m_mutex_sharedmemory.lock();
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
    m_mutex_sharedmemory.unlock();
    pmdmntExit();
}

void HidSharedMemoryManager::DumpProcessesAndMemoryAddr()
{
    ::syscon::logger::LogDebug("_____________________________________________________________________________________");
    ::syscon::logger::LogDebug("|     Program ID     |     Process ID     |      FakeAddr      |      RealAddr      |");
    m_mutex_sharedmemory.lock();
    for (const auto &entry : m_sharedmemory_entry_list)
    {
        ::syscon::logger::LogDebug("| 0x%016" PRIx64 " | 0x%016" PRIx64 " | 0x%016" PRIx64 " | 0x%016" PRIx64 " |",
                                   entry->GetProgramId(), entry->GetProcessId(), entry->GetFakeAddr(), entry->GetRealAddr());
    }
    m_mutex_sharedmemory.unlock();
    ::syscon::logger::LogDebug("_____________________________________________________________________________________");
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

    Result rc = threadCreate(&m_thread, &HidSharedMemoryManagerThreadFunc, this, m_thread_stack, sizeof(m_thread_stack), 38, 3 /* On CPU 3 responsible for input */);
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

void HidSharedMemoryManager::Mirror(HidSharedMemoryEntry &entry)
{
    HidSharedMemory *real = entry.GetRealAddr();
    HidSharedMemory *fake = entry.GetFakeAddr();

    memcpy_64(fake, real, NpadOffset);
    memcpy_64(ByteAddr(fake, AfterNpadOffset), ByteAddr(real, AfterNpadOffset), AfterNpadSize);

    for (size_t i = 0; i < NpadEntryCount; i++)
    {
        // A slot sys-con drives is its own: mirroring it would wipe the virtual pad.
        if (i < m_controller_list.size() && IsPlayerIndexOwned(i))
            continue;

        // An npad the console never populated stays zeroed on both sides. Skipping those
        // is what keeps the mirror cheap enough to run at the 200 Hz hid itself ticks at.
        if (real->npad.entries[i].internal_state.style_set == 0 && fake->npad.entries[i].internal_state.style_set == 0)
            continue;

        memcpy_64(&fake->npad.entries[i], &real->npad.entries[i], sizeof(HidNpadSharedMemoryEntry));
    }
}

void HidSharedMemoryManager::OnRun()
{
    ::syscon::logger::LogDebug("HidSharedMemoryManager::OnRun running...");

    while (m_running)
    {
        auto startTimer = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::recursive_mutex> controller_lock(m_mutex_controller);
            std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);

            for (const auto &entry : m_sharedmemory_entry_list)
            {
                Mirror(*entry);

                for (const auto &controller : m_controller_list)
                {
                    if (controller != nullptr)
                        controller->Publish(*entry);
                }
            }
        }

        s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();
        if (execution_time_us < POLLING_FREQUENCY_US)
            svcSleepThread((POLLING_FREQUENCY_US - execution_time_us) * 1000); // Convert to nanoseconds
    }
}

/* ---------------------------------------- */

HidSharedMemoryController::HidSharedMemoryController(uint8_t player_idx)
    : m_player_idx(player_idx),
      m_sampling_number(0),
      m_buttons(0),
      m_analog_stick_l{},
      m_analog_stick_r{}
{
}

/* ---------------------------------------- */

HidSharedMemoryController::~HidSharedMemoryController()
{
}

/* ---------------------------------------- */

void HidSharedMemoryController::Initialize(HidNpadInternalState *internal_state)
{
    ::syscon::logger::LogDebug("HidSharedMemoryController::Initialize initializing player %d ...", m_player_idx + 1);

    memset(internal_state, 0, sizeof(HidNpadInternalState));

    internal_state->style_set = HidNpadStyleTag_NpadSystemExt | HidNpadStyleTag_NpadFullKey;
    internal_state->joy_assignment_mode = 0;
    internal_state->full_key_color.attribute = HidColorAttribute_Ok;
    internal_state->full_key_color.full_key.main = 0xFF0000FF; // BodyColor
    internal_state->full_key_color.full_key.sub = 0xFF0000FF;  // ButtonColor

    internal_state->full_key_lifo.header.buffer_count = 17;
    internal_state->system_ext_lifo.header.buffer_count = 17;

    internal_state->device_type = HidDeviceTypeBits_FullKey;
    internal_state->system_properties.is_abxy_button_oriented = 1;
    internal_state->system_properties.is_plus_available = 1;
    internal_state->system_properties.is_minus_available = 1;
    internal_state->system_properties.is_directional_buttons_available = 1;
    internal_state->battery_level[0] = 4; // Set battery charge to full.
    internal_state->battery_level[1] = 4; // Set battery charge to full.
    internal_state->battery_level[2] = 4; // Set battery charge to full.
    internal_state->applet_footer_ui_type = HidAppletFooterUiType_SwitchProController;
}

/* ---------------------------------------- */

static void AppendNpadState(HidNpadCommonLifo *lifo, const HidNpadCommonState &state)
{
    u64 current_tail = lifo->header.tail + 1;
    if (current_tail >= lifo->header.buffer_count)
        current_tail = 0;

    lifo->storage[current_tail].sampling_number = state.sampling_number;
    lifo->storage[current_tail].state = state;

    __atomic_store_n(&lifo->header.tail, current_tail, __ATOMIC_RELEASE);

    if (lifo->header.count < lifo->header.buffer_count)
        __atomic_store_n(&lifo->header.count, lifo->header.count + 1, __ATOMIC_RELEASE);
}

/*
    Called from the manager thread, once per tick per client, whether or not the pad
    reported anything new: a real controller keeps sampling at a fixed rate and the
    console treats a lifo that stops advancing as a pad that went away.
*/
void HidSharedMemoryController::Publish(HidSharedMemoryEntry &entry)
{
    HidNpadInternalState *internal_state = &entry.GetFakeAddr()->npad.entries[m_player_idx].internal_state;

    if (internal_state->style_set == 0)
        Initialize(internal_state);

    HidNpadCommonState state{};
    state.sampling_number = m_sampling_number;
    state.buttons = m_buttons;
    state.analog_stick_l = m_analog_stick_l;
    state.analog_stick_r = m_analog_stick_r;
    state.attributes = HidNpadAttribute_IsConnected | HidNpadAttribute_IsWired;

    AppendNpadState(&internal_state->full_key_lifo, state);
    AppendNpadState(&internal_state->system_ext_lifo, state);

    m_sampling_number++;
}

/* ---------------------------------------- */

void HidSharedMemoryController::Clear(HidSharedMemoryEntry &entry)
{
    memset(&entry.GetFakeAddr()->npad.entries[m_player_idx].internal_state, 0, sizeof(HidNpadInternalState));
}

/* ---------------------------------------- */

Result HidSharedMemoryController::Update(u64 buttons, const HidAnalogStickState &analog_stick_l, const HidAnalogStickState &analog_stick_r)
{
    std::lock_guard<std::recursive_mutex> lock(g_HidSharedMemoryManager.m_mutex_controller);

    m_buttons = buttons;
    m_analog_stick_l = analog_stick_l;
    m_analog_stick_r = analog_stick_r;

    return 0;
}
