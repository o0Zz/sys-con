#include "SwitchMITMManager.h"
#include "SwitchVirtualHid.h"
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

    constexpr size_t MouseOffset = offsetof(HidSharedMemory, mouse);
    constexpr size_t MouseSize = sizeof(HidMouseSharedMemoryFormat);
    constexpr size_t KeyboardOffset = offsetof(HidSharedMemory, keyboard);
    constexpr size_t KeyboardSize = sizeof(HidKeyboardSharedMemoryFormat);
    constexpr size_t AfterKeyboardOffset = KeyboardOffset + KeyboardSize;

    static_assert(KeyboardOffset == MouseOffset + MouseSize, "mouse and keyboard are expected to be adjacent");
    // memcpy_64 falls off its fast path on anything else.
    static_assert(MouseOffset % 8 == 0 && MouseSize % 8 == 0 && KeyboardSize % 8 == 0 && (NpadOffset - AfterKeyboardOffset) % 8 == 0);

    constexpr size_t LifoBufferCount = 17;

    u8 *ByteAddr(HidSharedMemory *shmem, size_t offset)
    {
        return reinterpret_cast<u8 *>(shmem) + offset;
    }
} // namespace

/*
    One fake shared memory for every mitm'd client, rather than one each.
    svcCreateSharedMemory charges 256 KiB against the shared system resource limit - sys-con
    has no reservation of its own (pool_partition 2, system_resource_size 0) - so a
    per-client allocation starts failing with 0x00010801 (LimitReached) as soon as the pool
    is tight, which leaves the client with no HID shared memory and takes the console down.
    Every client can share one: the contents reflect the same physical controllers plus the
    pads sys-con injects. Allocating it once at Start() also claims the memory while the
    pool is still free, instead of mid-session when it is not.
*/
static ::SharedMemory g_fake_shared_memory{};
static bool g_fake_shared_memory_seeded = false;

static Result CreateFakeSharedMemory()
{
    if (g_fake_shared_memory.handle != INVALID_HANDLE)
        return 0;

    Result rc = shmemCreate(&g_fake_shared_memory, HID_SHARED_MEMORY_SIZE, Perm_Rw, Perm_R);
    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("HidSharedMemory failed to create the shared fake memory: 0x%08X (Mod:%d - Desc:%d)", rc, R_MODULE(rc), R_DESCRIPTION(rc));
        g_fake_shared_memory = ::SharedMemory{};
        return rc;
    }

    rc = shmemMap(&g_fake_shared_memory);
    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("HidSharedMemory failed to map the shared fake memory: 0x%08X (Mod:%d - Desc:%d)", rc, R_MODULE(rc), R_DESCRIPTION(rc));
        shmemClose(&g_fake_shared_memory);
        g_fake_shared_memory = ::SharedMemory{};
        return rc;
    }

    ::syscon::logger::LogInfo("HidSharedMemory shared fake memory ready (FakeAddr: %p)", shmemGetAddr(&g_fake_shared_memory));
    return 0;
}

static void DestroyFakeSharedMemory()
{
    if (g_fake_shared_memory.map_addr != nullptr)
        shmemUnmap(&g_fake_shared_memory);
    if (g_fake_shared_memory.handle != INVALID_HANDLE)
        shmemClose(&g_fake_shared_memory);

    g_fake_shared_memory = ::SharedMemory{};
    g_fake_shared_memory_seeded = false;
}

// The npad table of the one shared fake; null until CreateFakeSharedMemory() succeeds.
static HidNpadSharedMemoryEntry *FakeNpadEntries()
{
    HidSharedMemory *fake = static_cast<HidSharedMemory *>(shmemGetAddr(&g_fake_shared_memory));
    return fake != nullptr ? fake->npad.entries : nullptr;
}

static HidSharedMemory *FakeSharedMemory()
{
    return static_cast<HidSharedMemory *>(shmemGetAddr(&g_fake_shared_memory));
}

template <typename Lifo, typename State>
static void AppendLifoState(Lifo *lifo, const State &state)
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

template <typename Lifo>
static void InitializeLifo(Lifo *lifo)
{
    memset(lifo, 0, sizeof(*lifo));
    lifo->header.buffer_count = LifoBufferCount;
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

    // Normally already created by Start(); this only has to do anything if that failed.
    m_status = CreateFakeSharedMemory();
    if (R_FAILED(m_status))
        return;

    ::syscon::logger::LogInfo("HidSharedMemoryEntry created successfully (Process id: 0x%016" PRIx64 ", RealAddr: %p, FakeAddr: %p)", m_process_id, GetRealAddr(), GetFakeAddr());

    /*
        Seed from the first client only. The shared fake is already live for everyone else,
        and the mirror thread keeps it current - copying over it again here would wipe the
        npad slots sys-con has injected for the clients that are already running.
    */
    if (!g_fake_shared_memory_seeded)
    {
        memcpy_64(GetFakeAddr(), GetRealAddr(), HID_SHARED_MEMORY_SIZE);
        g_fake_shared_memory_seeded = true;
    }
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
    return g_fake_shared_memory;
}

::HidSharedMemory *HidSharedMemoryEntry::GetRealAddr()
{
    return (HidSharedMemory *)shmemGetAddr(&m_real_shared_memory);
}

::HidSharedMemory *HidSharedMemoryEntry::GetFakeAddr()
{
    return (HidSharedMemory *)shmemGetAddr(&g_fake_shared_memory);
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

    std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);
    m_controller_list[player_idx]->Clear();

    ::syscon::logger::LogInfo("HidSharedMemoryManager attached a controller on player %d", player_idx + 1);
    return m_controller_list[player_idx];
}

void HidSharedMemoryManager::DetachController(std::shared_ptr<HidSharedMemoryController> controller)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex_controller);

    for (size_t i = 0; i < m_controller_list.size(); i++)
    {
        if (m_controller_list[i] != controller)
            continue;

        m_player_owned[i].store(false, std::memory_order_relaxed);
        ClearVibration(i);

        std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);
        controller->Clear();

        ::syscon::logger::LogInfo("HidSharedMemoryManager detached the controller of player %d", (int)i + 1);
        m_controller_list[i] = nullptr;
        return;
    }
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

    std::lock_guard<std::recursive_mutex> lock(m_mutex_controller);

    /*
        The entry starts life as a byte copy of the real shared memory, so the slots
        sys-con drives still hold whatever the console had there. Clearing them is what
        makes Publish() see style_set == 0 and lay the virtual npad out from scratch.
    */
    for (const auto &controller : m_controller_list)
    {
        if (controller != nullptr)
            controller->Clear();
    }

    if (m_keyboard_owned.load())
        m_keyboard.Reset();

    if (m_mouse_owned.load())
        m_mouse.Reset();

    m_mutex_sharedmemory.lock();
    m_sharedmemory_entry_list.push_back(entry);
    m_mutex_sharedmemory.unlock();

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
        Claim the 256 KiB now, while the system memory pool is still free. Leaving it until
        a client arrives means asking once an applet has already taken the slack, which is
        when svcCreateSharedMemory starts returning 0x00010801 (LimitReached).
    */
    CreateFakeSharedMemory();

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

    DestroyFakeSharedMemory();
}

void HidSharedMemoryManager::Mirror(HidSharedMemoryEntry &entry)
{
    HidSharedMemory *real = entry.GetRealAddr();
    HidSharedMemory *fake = entry.GetFakeAddr();

    memcpy_64(fake, real, MouseOffset);

    // A section sys-con drives is its own, exactly as an owned npad slot is below.
    if (!m_mouse_owned.load(std::memory_order_relaxed))
        memcpy_64(ByteAddr(fake, MouseOffset), ByteAddr(real, MouseOffset), MouseSize);

    if (!m_keyboard_owned.load(std::memory_order_relaxed))
        memcpy_64(ByteAddr(fake, KeyboardOffset), ByteAddr(real, KeyboardOffset), KeyboardSize);

    memcpy_64(ByteAddr(fake, AfterKeyboardOffset), ByteAddr(real, AfterKeyboardOffset), NpadOffset - AfterKeyboardOffset);
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

            /*
                One fake serves every client, so mirror once from any client's real view -
                they all reflect the same physical controllers - and publish the virtual
                pads once, rather than repeating both for each client.
            */
            if (!m_sharedmemory_entry_list.empty())
            {
                Mirror(*m_sharedmemory_entry_list.front());

                for (const auto &controller : m_controller_list)
                {
                    if (controller != nullptr)
                        controller->Publish();
                }

                if (m_keyboard_owned.load(std::memory_order_relaxed))
                    m_keyboard.Publish();

                if (m_mouse_owned.load(std::memory_order_relaxed))
                    m_mouse.Publish();
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
      m_buttons(0),
      m_analog_stick_l{},
      m_analog_stick_r{}
{
}

/* ---------------------------------------- */

void HidSharedMemoryController::Initialize(HidNpadInternalState *internal_state)
{
    ::syscon::logger::LogDebug("HidSharedMemoryController::Initialize initializing player %d ...", m_player_idx + 1);

    memset(internal_state, 0, sizeof(HidNpadInternalState));

    internal_state->style_set = HidNpadStyleTag_NpadSystemExt | HidNpadStyleTag_NpadFullKey;
    internal_state->joy_assignment_mode = 0;
    // The same colours the pad was created with, so a mitm'd applet draws it the way the
    // Controllers menu - which reads the real hid - already does.
    internal_state->full_key_color.attribute = HidColorAttribute_Ok;
    internal_state->full_key_color.full_key.main = m_body_color;
    internal_state->full_key_color.full_key.sub = m_buttons_color;

    internal_state->full_key_lifo.header.buffer_count = LifoBufferCount;
    internal_state->system_ext_lifo.header.buffer_count = LifoBufferCount;

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

/*
    Called from the manager thread, once per tick per client, whether or not the pad
    reported anything new: a real controller keeps sampling at a fixed rate and the
    console treats a lifo that stops advancing as a pad that went away.
*/
void HidSharedMemoryController::Publish()
{
    HidNpadInternalState *internal_state = &FakeNpadEntries()[m_player_idx].internal_state;

    if (internal_state->style_set == 0)
        Initialize(internal_state);

    HidNpadCommonState state{};
    state.sampling_number = m_sampling_number;
    state.buttons = m_buttons;
    state.analog_stick_l = m_analog_stick_l;
    state.analog_stick_r = m_analog_stick_r;
    state.attributes = HidNpadAttribute_IsConnected | HidNpadAttribute_IsWired;

    AppendLifoState(&internal_state->full_key_lifo, state);
    AppendLifoState(&internal_state->system_ext_lifo, state);

    m_sampling_number++;
}

/* ---------------------------------------- */

void HidSharedMemoryController::Clear()
{
    memset(&FakeNpadEntries()[m_player_idx].internal_state, 0, sizeof(HidNpadInternalState));
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

/* ---------------------------------------- */

void HidSharedMemoryController::GetRumble(float *amp_high, float *amp_low) const
{
    g_HidSharedMemoryManager.GetRumble(m_player_idx, amp_high, amp_low);
}

/* ---------------------------------------- */

void HidSharedMemoryKeyboard::Reset()
{
    HidSharedMemory *fake = FakeSharedMemory();
    if (fake == nullptr)
        return;

    InitializeLifo(&fake->keyboard.lifo);
    m_sampling_number = 0;
}

void HidSharedMemoryKeyboard::Publish()
{
    HidSharedMemory *fake = FakeSharedMemory();
    if (fake == nullptr)
        return;

    HidKeyboardState state{};
    if (!::syscon::hid::VirtualKeyboard::Get().Compose(&state))
        return;

    state.sampling_number = m_sampling_number++;
    AppendLifoState(&fake->keyboard.lifo, state);
}

void HidSharedMemoryMouse::Reset()
{
    HidSharedMemory *fake = FakeSharedMemory();
    if (fake == nullptr)
        return;

    InitializeLifo(&fake->mouse.lifo);
    m_sampling_number = 0;
}

void HidSharedMemoryMouse::Publish()
{
    HidSharedMemory *fake = FakeSharedMemory();
    if (fake == nullptr)
        return;

    HidMouseState state{};
    if (!::syscon::hid::VirtualMouse::Get().Drain(&state))
        return;

    state.sampling_number = m_sampling_number++;
    AppendLifoState(&fake->mouse.lifo, state);
}

/* ---------------------------------------- */

Result HidSharedMemoryManager::AttachKeyboard()
{
    std::lock_guard<std::recursive_mutex> controller_lock(m_mutex_controller);
    std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);

    if (m_keyboard_owned.load())
    {
        ::syscon::logger::LogError("HidSharedMemoryManager::AttachKeyboard a keyboard is already attached !");
        return MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized);
    }

    m_keyboard_owned.store(true);
    m_keyboard.Reset();

    ::syscon::logger::LogInfo("HidSharedMemoryManager::AttachKeyboard keyboard attached !");
    return 0;
}

void HidSharedMemoryManager::DetachKeyboard()
{
    std::lock_guard<std::recursive_mutex> controller_lock(m_mutex_controller);
    std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);

    // Stop owning it first: the next mirror tick then restores the console's own keyboard
    // view, so a real USB keyboard keeps working once sys-con's is unplugged.
    m_keyboard_owned.store(false);
}

Result HidSharedMemoryManager::AttachMouse()
{
    std::lock_guard<std::recursive_mutex> controller_lock(m_mutex_controller);
    std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);

    if (m_mouse_owned.load())
    {
        ::syscon::logger::LogError("HidSharedMemoryManager::AttachMouse a mouse is already attached !");
        return MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized);
    }

    m_mouse_owned.store(true);
    m_mouse.Reset();

    ::syscon::logger::LogInfo("HidSharedMemoryManager::AttachMouse mouse attached !");
    return 0;
}

void HidSharedMemoryManager::DetachMouse()
{
    std::lock_guard<std::recursive_mutex> controller_lock(m_mutex_controller);
    std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);

    m_mouse_owned.store(false);
}
