#include "SwitchMITMManager.h"
#include "SwitchLogger.h"
#include <string.h> // memcpy
#include <algorithm>
#include <cinttypes>
#include <atomic>
#include <chrono>

#define HID_SHARED_MEMORY_SIZE 0x40000 // 256 KiB
#define POLLING_FREQUENCY_US   5000    // 5ms // Official software ticks 200 times/second

static HidSharedMemoryManager g_HidSharedMemoryManager;

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

/*
    One fake shared memory per view, not one per client.
    svcCreateSharedMemory charges 256 KiB against the shared system resource limit - sys-con
    has no reservation of its own (pool_partition 2, system_resource_size 0) - so a
    per-client allocation starts failing with 0x00010801 (LimitReached) as soon as the pool
    is tight, which leaves the client with no HID shared memory and takes the console down.
    Both are allocated at Start(), while the pool is still free.

    Two views rather than one because the real hid writes each aruid only the npad styles
    that client declared through SetSupportedNpadStyleSet. The system applets declare
    SystemExt, and an application mirrored from one of them sees it on the physical
    controllers - which aborts nn::hid in a title that never asked for it (SSBU, 2162-0001).
    Only one application runs at a time, so it gets a fake of its own mirrored from its own
    real view, and every system applet shares the other.
*/
namespace
{
    constexpr u64 ApplicationProgramIdStart = 0x0100000000010000ul;

    struct FakeSharedMemory
    {
        ::SharedMemory shmem;
        const char *name;
    };

    std::array<FakeSharedMemory, HidFakeViewCount> g_fake_shared_memory{{
        {::SharedMemory{}, "system"},
        {::SharedMemory{}, "application"},
    }};

    HidFakeView ViewForProgram(u64 program_id)
    {
        return program_id >= ApplicationProgramIdStart ? HidFakeView::Application : HidFakeView::System;
    }

    FakeSharedMemory &Fake(HidFakeView view)
    {
        return g_fake_shared_memory[static_cast<size_t>(view)];
    }

    HidSharedMemory *FakeAddr(HidFakeView view)
    {
        return static_cast<HidSharedMemory *>(shmemGetAddr(&Fake(view).shmem));
    }

    // Null for a view whose fake could not be allocated; that failure is logged once, at
    // allocation, and no client is ever handed that view.
    HidNpadInternalState *FakeNpadState(HidFakeView view, uint8_t player_idx)
    {
        HidSharedMemory *fake = FakeAddr(view);
        return fake != nullptr ? &fake->npad.entries[player_idx].internal_state : nullptr;
    }
} // namespace

static Result CreateFakeSharedMemory(HidFakeView view)
{
    FakeSharedMemory &fake = Fake(view);
    if (fake.shmem.handle != INVALID_HANDLE)
        return 0;

    Result rc = shmemCreate(&fake.shmem, HID_SHARED_MEMORY_SIZE, Perm_Rw, Perm_R);
    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("HidSharedMemory failed to create the %s fake memory: 0x%08X (Mod:%d - Desc:%d)", fake.name, rc, R_MODULE(rc), R_DESCRIPTION(rc));
        fake.shmem = ::SharedMemory{};
        return rc;
    }

    rc = shmemMap(&fake.shmem);
    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("HidSharedMemory failed to map the %s fake memory: 0x%08X (Mod:%d - Desc:%d)", fake.name, rc, R_MODULE(rc), R_DESCRIPTION(rc));
        shmemClose(&fake.shmem);
        fake.shmem = ::SharedMemory{};
        return rc;
    }

    ::syscon::logger::LogInfo("HidSharedMemory %s fake memory ready (FakeAddr: %p)", fake.name, shmemGetAddr(&fake.shmem));
    return 0;
}

static void DestroyFakeSharedMemories()
{
    for (FakeSharedMemory &fake : g_fake_shared_memory)
    {
        if (fake.shmem.map_addr != nullptr)
            shmemUnmap(&fake.shmem);
        if (fake.shmem.handle != INVALID_HANDLE)
            shmemClose(&fake.shmem);

        fake.shmem = ::SharedMemory{};
    }
}

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
    : m_process_id(processId), m_program_id(programId), m_view(ViewForProgram(programId))
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

    // Normally already created by Start(); this only has to do anything if that failed.
    m_status = CreateFakeSharedMemory(m_view);
    if (R_FAILED(m_status))
        return;

    ::syscon::logger::LogInfo("HidSharedMemoryEntry created successfully (Process id: 0x%016" PRIx64 ", View: %s, RealAddr: %p, FakeAddr: %p)", m_process_id, Fake(m_view).name, GetRealAddr(), GetFakeAddr());
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

    if (serviceIsActive(&m_appletresource))
        serviceClose(&m_appletresource);
}

const ::SharedMemory &HidSharedMemoryEntry::GetSharedMemoryHandle() const
{
    return Fake(m_view).shmem;
}

::HidSharedMemory *HidSharedMemoryEntry::GetRealAddr()
{
    return (HidSharedMemory *)shmemGetAddr(&m_real_shared_memory);
}

::HidSharedMemory *HidSharedMemoryEntry::GetFakeAddr()
{
    return FakeAddr(m_view);
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
    return player_idx < m_player_owned.size() && m_player_owned[player_idx].load(std::memory_order_relaxed);
}

void HidSharedMemoryManager::SetVibration(uint8_t player_idx, uint8_t device_idx, const HidVibrationValue &value)
{
    VibrationSlot &slot = m_vibration[(player_idx * HidSharedMemoryController::VibrationDeviceCount) + (device_idx % HidSharedMemoryController::VibrationDeviceCount)];

    slot.amp_low.store(value.amp_low, std::memory_order_relaxed);
    slot.amp_high.store(value.amp_high, std::memory_order_relaxed);
    slot.freq_low.store(value.freq_low, std::memory_order_relaxed);
    slot.freq_high.store(value.freq_high, std::memory_order_relaxed);
}

HidVibrationValue HidSharedMemoryManager::GetVibration(uint8_t player_idx, uint8_t device_idx) const
{
    const VibrationSlot &slot = m_vibration[(player_idx * HidSharedMemoryController::VibrationDeviceCount) + (device_idx % HidSharedMemoryController::VibrationDeviceCount)];

    return HidVibrationValue{
        .amp_low = slot.amp_low.load(std::memory_order_relaxed),
        .freq_low = slot.freq_low.load(std::memory_order_relaxed),
        .amp_high = slot.amp_high.load(std::memory_order_relaxed),
        .freq_high = slot.freq_high.load(std::memory_order_relaxed),
    };
}

/*
    A Switch pad has one actuator per grip and each carries a low and a high band, while a
    driver takes a single pair of amplitudes. The loudest band of either actuator is what the
    player feels, so that is what is handed down.
*/
void HidSharedMemoryManager::GetRumble(uint8_t player_idx, float *amp_high, float *amp_low) const
{
    *amp_high = 0.0f;
    *amp_low = 0.0f;

    for (uint8_t device_idx = 0; device_idx < HidSharedMemoryController::VibrationDeviceCount; device_idx++)
    {
        const VibrationSlot &slot = m_vibration[(player_idx * HidSharedMemoryController::VibrationDeviceCount) + device_idx];

        *amp_high = std::max(*amp_high, slot.amp_high.load(std::memory_order_relaxed));
        *amp_low = std::max(*amp_low, slot.amp_low.load(std::memory_order_relaxed));
    }
}

void HidSharedMemoryManager::ClearVibration(uint8_t player_idx)
{
    for (uint8_t device_idx = 0; device_idx < HidSharedMemoryController::VibrationDeviceCount; device_idx++)
        SetVibration(player_idx, device_idx, HidVibrationValue{});
}

std::shared_ptr<HidSharedMemoryController> HidSharedMemoryManager::AttachControllerAt(uint8_t player_idx, u32 body_color, u32 buttons_color)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex_controller);

    if (player_idx >= m_controller_list.size())
    {
        ::syscon::logger::LogError("HidSharedMemoryManager cannot attach a controller on player %d, out of range !", player_idx + 1);
        return nullptr;
    }

    if (IsPlayerIndexOwned(player_idx))
    {
        ::syscon::logger::LogError("HidSharedMemoryManager already owns player %d, controller not attached !", player_idx + 1);
        return nullptr;
    }

    m_controller_list[player_idx] = std::make_shared<HidSharedMemoryController>(player_idx, body_color, buttons_color);
    ClearVibration(player_idx);
    m_player_owned[player_idx].store(true, std::memory_order_relaxed);

    {
        std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);
        m_controller_list[player_idx]->Clear();
    }

    ::syscon::logger::LogInfo("HidSharedMemoryManager attached a controller on player %d", player_idx + 1);
    return m_controller_list[player_idx];
}

/*
    Every line logged here is an SD write of several milliseconds, and both of these mutexes
    are wanted by the mirror thread every 5 ms and by the MITM server thread on any client
    request - which is every hid client on the console. So the locks cover the state change
    and nothing else, exactly as the garbage collector does.
*/
void HidSharedMemoryManager::DetachController(std::shared_ptr<HidSharedMemoryController> controller)
{
    ::syscon::logger::LogInfo("HidSharedMemoryManager detaching a controller ...");

    int player_idx = -1;

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex_controller);

        for (size_t i = 0; i < m_controller_list.size(); i++)
        {
            if (m_controller_list[i] != controller)
                continue;

            m_player_owned[i].store(false, std::memory_order_relaxed);
            ClearVibration(i);

            {
                std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);
                controller->Clear();
            }

            m_controller_list[i] = nullptr;
            player_idx = (int)i;
            break;
        }
    }

    if (player_idx >= 0)
        ::syscon::logger::LogInfo("HidSharedMemoryManager detached the controller of player %d", player_idx + 1);
}

std::shared_ptr<HidSharedMemoryEntry> HidSharedMemoryManager::CreateIfNotExists(::Service *hid_service, u64 processId, u64 programId)
{
    /*
        Reclaim before allocating, not after. Every mitm'd process costs a 256 KiB fake
        shared memory plus a mapping of the real one, and applets come and go constantly -
        so without this the list only grows and shmemCreate eventually fails with
        0xE401, leaving every later applet with no HID shared memory at all.
        Running it from Add() would be too late: the allocation that needs the room
        happens in the constructor below.
    */
    RunGarbageCollector();

    std::shared_ptr<HidSharedMemoryEntry> entry = std::make_shared<HidSharedMemoryEntry>(hid_service, processId, programId);

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

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex_controller);
        std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);

        /*
            Seed a view from its first live client only. Once one is running the mirror
            thread keeps the fake current, and copying over it again would hand the clients
            already reading it a jump in every lifo.
        */
        if (FindEntry(entry->GetView()) == nullptr)
            memcpy_64(entry->GetFakeAddr(), entry->GetRealAddr(), HID_SHARED_MEMORY_SIZE);

        /*
            The seed is a byte copy of the real shared memory, so the slots sys-con drives
            still hold whatever the console had there. Clearing them is what makes Publish()
            see style_set == 0 and lay the virtual npad out from scratch.
        */
        for (const auto &controller : m_controller_list)
        {
            if (controller != nullptr)
                controller->Clear();
        }

        m_sharedmemory_entry_list.push_back(entry);
    }

    DumpProcessesAndMemoryAddr();

    return 0;
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

    /*
        The mirror thread wants m_mutex_sharedmemory every 5 ms, and both the pm queries
        below and LogWarning (an SD write, ~7 ms) are far too slow to do while holding it.
        So: snapshot under the lock, decide unlocked, then take it again just to erase.
    */
    std::vector<std::shared_ptr<HidSharedMemoryEntry>> snapshot;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex_sharedmemory);
        snapshot = m_sharedmemory_entry_list;
    }

    std::vector<std::shared_ptr<HidSharedMemoryEntry>> dead;
    for (const auto &entry : snapshot)
    {
        u64 pid_out = 0;

        Result ret = pmdmntGetProcessId(&pid_out, entry->GetProgramId());
        if (R_SUCCEEDED(ret) && pid_out == entry->GetProcessId())
            continue;

        ::syscon::logger::LogWarning("HidSharedMemoryManager Process id 0x%016" PRIx64 " is not running anymore, remove it ! (Ret: 0x%08X - Mod:%d - Desc:%d)", entry->GetProcessId(), ret, R_MODULE(ret), R_DESCRIPTION(ret));
        dead.push_back(entry);
    }

    pmdmntExit();

    if (dead.empty())
        return;

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex_sharedmemory);
        for (const auto &entry : dead)
        {
            for (auto it = m_sharedmemory_entry_list.begin(); it != m_sharedmemory_entry_list.end(); ++it)
            {
                if (*it == entry)
                {
                    m_sharedmemory_entry_list.erase(it);
                    break;
                }
            }
        }
    }

    /*
        `dead` drops the last references here, outside every lock: ~HidSharedMemoryEntry
        unmaps and closes two shared memories and closes an IAppletResource session, none
        of which should run with the mirror thread blocked behind it.
    */
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

    /*
        Claim the 2 x 256 KiB now, while the system memory pool is still free. Leaving it until
        a client arrives means asking once an applet has already taken the slack, which is
        when svcCreateSharedMemory starts returning 0x00010801 (LimitReached).
    */
    for (size_t view = 0; view < HidFakeViewCount; view++)
        CreateFakeSharedMemory(static_cast<HidFakeView>(view));

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

    DestroyFakeSharedMemories();
}

std::shared_ptr<HidSharedMemoryEntry> HidSharedMemoryManager::FindEntry(HidFakeView view) const
{
    for (const auto &entry : m_sharedmemory_entry_list)
    {
        if (entry->GetView() == view)
            return entry;
    }
    return nullptr;
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

            for (size_t view = 0; view < HidFakeViewCount; view++)
            {
                std::shared_ptr<HidSharedMemoryEntry> source = FindEntry(static_cast<HidFakeView>(view));
                if (source != nullptr)
                    Mirror(*source);
            }

            for (const auto &controller : m_controller_list)
            {
                if (controller != nullptr)
                    controller->Publish();
            }
        }

        s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();
        if (execution_time_us < POLLING_FREQUENCY_US)
            svcSleepThread((POLLING_FREQUENCY_US - execution_time_us) * 1000); // Convert to nanoseconds
    }
}

/* ---------------------------------------- */

HidSharedMemoryController::HidSharedMemoryController(uint8_t player_idx, u32 body_color, u32 buttons_color)
    : m_player_idx(player_idx),
      m_body_color(body_color),
      m_buttons_color(buttons_color),
      m_sampling_number(0),
      m_state{}
{
}

/* ---------------------------------------- */

void HidSharedMemoryController::Initialize(HidNpadInternalState *internal_state)
{
    ::syscon::logger::LogDebug("HidSharedMemoryController::Initialize initializing player %d ...", m_player_idx + 1);

    memset(internal_state, 0, sizeof(HidNpadInternalState));

    internal_state->joy_assignment_mode = 0;
    // The same colours the pad was created with, so a mitm'd applet draws it the way the
    // Controllers menu - which reads the real hid - already does.
    internal_state->full_key_color.attribute = HidColorAttribute_Ok;
    internal_state->full_key_color.full_key.main = m_body_color;
    internal_state->full_key_color.full_key.sub = m_buttons_color;

    internal_state->full_key_lifo.header.buffer_count = 17;
    internal_state->system_ext_lifo.header.buffer_count = 17;
    internal_state->full_key_six_axis_sensor_lifo.header.buffer_count = 17;

    internal_state->device_type = HidDeviceTypeBits_FullKey;
    internal_state->system_properties.is_abxy_button_oriented = 1;
    internal_state->system_properties.is_plus_available = 1;
    internal_state->system_properties.is_minus_available = 1;
    internal_state->system_properties.is_directional_buttons_available = 1;
    internal_state->battery_level[0] = 4; // Set battery charge to full.
    internal_state->battery_level[1] = 4; // Set battery charge to full.
    internal_state->battery_level[2] = 4; // Set battery charge to full.
    internal_state->applet_footer_ui_type = HidAppletFooterUiType_SwitchProController;

    /*
        Last, and on its own: style_set is what tells a reader the slot holds a pad, and
        everything it will then walk - the lifo buffer counts above most of all - has to be in
        place before it does.

        FullKey alone. SystemExt is a system style, and the real hid only ever reports a
        style the client itself declared through SetSupportedNpadStyleSet; one fake shared
        memory serves every mitm'd process here, so a bit set for the system applets is a bit
        an application reads too, and nn::hid aborts in a title that never asked for it
        (SSBU). The system applets are served by system_ext_lifo being filled below, which
        costs them nothing.
    */
    __atomic_store_n(&internal_state->style_set, static_cast<u32>(HidNpadStyleTag_NpadFullKey), __ATOMIC_RELEASE);
}

/* ---------------------------------------- */

template <typename Lifo>
static void EmptyLifo(Lifo *lifo)
{
    __atomic_store_n(&lifo->header.count, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&lifo->header.tail, 0u, __ATOMIC_RELEASE);
}

template <typename Lifo, typename State>
static void AppendState(Lifo *lifo, const State &state)
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
void HidSharedMemoryController::Publish()
{
    HidNpadCommonState state{};
    state.sampling_number = m_sampling_number;
    state.buttons = m_state.buttons;
    state.analog_stick_l = m_state.analog_stick_l;
    state.analog_stick_r = m_state.analog_stick_r;
    state.attributes = HidNpadAttribute_IsConnected | HidNpadAttribute_IsWired;

    HidSixAxisSensorState six_axis{};
    if (m_state.has_motion)
    {
        m_motion.Step(m_state.angular_velocity, POLLING_FREQUENCY_US / 1e6f);

        six_axis.delta_time = POLLING_FREQUENCY_US * 1000ULL;
        six_axis.sampling_number = m_sampling_number;
        six_axis.acceleration = m_state.acceleration;
        six_axis.angular_velocity = m_state.angular_velocity;
        six_axis.angle = m_motion.GetAngle();
        six_axis.direction = m_motion.GetDirection();
        six_axis.attributes = HidSixAxisSensorAttribute_IsConnected;
    }

    // Every view gets the same samples under the same sampling numbers, and Clear() empties
    // them all at once: one m_sampling_number can then never show a gap in any of them.
    for (size_t view = 0; view < HidFakeViewCount; view++)
    {
        HidNpadInternalState *internal_state = FakeNpadState(static_cast<HidFakeView>(view), m_player_idx);
        if (internal_state == nullptr)
            continue;

        if (internal_state->style_set == 0)
            Initialize(internal_state);

        AppendState(&internal_state->full_key_lifo, state);

        // Filled for a style that is deliberately not announced: qlaunch, the Controllers
        // applet and profile select read this lifo and nothing else.
        AppendState(&internal_state->system_ext_lifo, state);

        // The npad and six-axis lifos are only ever filled together, so they share one
        // sampling number and neither can show the gap that hangs a reader.
        if (m_state.has_motion)
            AppendState(&internal_state->full_key_six_axis_sensor_lifo, six_axis);
    }

    m_sampling_number++;
}

/* ---------------------------------------- */

/*
    Retires the pad in the order a reader walks it: style_set first, so the clients that come
    after stop at the slot, then the lifos through their count. buffer_count is never unset -
    a reader already inside the lifo divides by it, and nn::hid restarts its read from the top
    whenever the sampling numbers it collected are not consecutive, so a header wiped
    underneath it is a read that need not terminate.
*/
void HidSharedMemoryController::Clear()
{
    for (size_t view = 0; view < HidFakeViewCount; view++)
    {
        HidNpadInternalState *internal_state = FakeNpadState(static_cast<HidFakeView>(view), m_player_idx);
        if (internal_state == nullptr)
            continue;

        __atomic_store_n(&internal_state->style_set, 0u, __ATOMIC_RELEASE);

        EmptyLifo(&internal_state->full_key_lifo);
        EmptyLifo(&internal_state->system_ext_lifo);
        EmptyLifo(&internal_state->full_key_six_axis_sensor_lifo);
    }

    m_sampling_number = 0;
}

/* ---------------------------------------- */

Result HidSharedMemoryController::Update(const SwitchPadState &state)
{
    std::lock_guard<std::recursive_mutex> lock(g_HidSharedMemoryManager.m_mutex_controller);

    m_state = state;

    return 0;
}

/* ---------------------------------------- */

void HidSharedMemoryController::GetRumble(float *amp_high, float *amp_low) const
{
    g_HidSharedMemoryManager.GetRumble(m_player_idx, amp_high, amp_low);
}
