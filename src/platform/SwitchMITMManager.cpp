#include "SwitchMITMManager.h"
#include "SwitchLogger.h"
#include <string.h> // memcpy
#include <algorithm>
#include <cinttypes>
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

    constexpr size_t SixAxisOffset = offsetof(HidNpadSharedMemoryEntry, internal_state.full_key_six_axis_sensor_lifo);
    constexpr size_t AfterSixAxisOffset = SixAxisOffset + sizeof(HidNpadSixAxisSensorLifo);

    constexpr u64 ApplicationProgramIdStart = 0x0100000000010000ul;
    constexpr u64 ApplicationProgramIdEnd = 0x01FFFFFFFFFFFFFFul;
    constexpr u64 AlbumProgramId = 0x010000000000100Dul;

    u8 *ByteAddr(void *base, size_t offset)
    {
        return static_cast<u8 *>(base) + offset;
    }

    const u8 *ByteAddr(const void *base, size_t offset)
    {
        return static_cast<const u8 *>(base) + offset;
    }
} // namespace

static_assert(NpadOffset % 8 == 0 && AfterNpadOffset % 8 == 0 && AfterNpadSize % 8 == 0);
static_assert(SixAxisOffset % 8 == 0 && AfterSixAxisOffset % 8 == 0);
static_assert(sizeof(HidNpadSharedMemoryEntry) % 8 == 0 && HID_SHARED_MEMORY_SIZE % 8 == 0);

/*
    From the end down, 8 bytes at a time. Every lifo starts with its header and every storage
    entry with its sampling number, so a reader never meets a tail pointing at a sample not
    copied yet, and a sample caught half-copied fails the reader's sampling-number check and is
    read again. Copied upwards, the new tail landed first and libnx walked stale samples whose
    numbers were not consecutive.
*/
static void MirrorCopy(void *dest, const void *src, size_t n)
{
    const volatile uint64_t *s = static_cast<const volatile uint64_t *>(src);
    volatile uint64_t *d = static_cast<volatile uint64_t *>(dest);
    for (size_t i = n / 8; i > 0; i--)
        d[i - 1] = s[i - 1];
}

/*
    One fake shared memory per view, not one per client.
    svcCreateSharedMemory charges 256 KiB against the shared system resource limit - sys-con
    has no reservation of its own (pool_partition 2, system_resource_size 0) - so a
    per-client allocation starts failing with 0x00010801 (LimitReached) as soon as the pool
    is tight. Both are allocated at Start(), while the pool is still free. Only one
    application and one Album run at a time, so a view only ever has one live client.
*/
namespace
{
    struct FakeSharedMemory
    {
        ::SharedMemory shmem;
        const char *name;
    };

    std::array<FakeSharedMemory, HidFakeViewCount> g_fake_shared_memory{{
        {::SharedMemory{}, "application"},
        {::SharedMemory{}, "album"},
    }};

    HidFakeView ViewForProgram(u64 program_id)
    {
        return program_id == AlbumProgramId ? HidFakeView::Album : HidFakeView::Application;
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
    HidNpadSixAxisSensorLifo *FakeSixAxisLifo(HidFakeView view, uint8_t player_idx)
    {
        HidSharedMemory *fake = FakeAddr(view);
        return fake != nullptr ? &fake->npad.entries[player_idx].internal_state.full_key_six_axis_sensor_lifo : nullptr;
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

/*
    hid keeps one shared memory per aruid, holding only the npad styles that aruid declared,
    only what it activated, and nothing at all while its input is taken away (an overlay).
    The fake is mirrored from the client's own: from sys-con's view - which never called
    SetSupportedNpadStyleSet, so SystemExt is on every npad - SSBU asserts in
    GetVibrationDeviceHandles. libnx can only send our own pid, so the request is built by hand
    with the client's pid carrying the mitm tag, exactly as a forwarded request does.
*/
static Result _HidCreateClientAppletResource(Service *srv, u64 aruid, u64 client_pid, Service *out_iappletresource)
{
    constexpr u64 MitmProcessIdTag = 0xFFFE000000000000ul;

    Service s = *srv;
    void *in = serviceMakeRequest(&s, 0, 0, sizeof(u64), true, SfBufferAttrs{}, nullptr, 0, nullptr, 0, nullptr);
    *static_cast<u64 *>(in) = aruid;
    *reinterpret_cast<u64 *>(static_cast<u8 *>(armGetTls()) + sizeof(HipcHeader) + sizeof(HipcSpecialHeader)) = MitmProcessIdTag | client_pid;

    Result rc = svcSendSyncRequest(s.session);
    if (R_FAILED(rc))
        return rc;

    void *out = nullptr;
    return serviceParseResponse(&s, 0, &out, 1, out_iappletresource, SfOutHandleAttrs{}, nullptr);
}

static Result _HidGetSharedMemoryHandle(Service *srv, Handle *handle_out)
{
    return serviceDispatch(srv, 0,
                           .out_handle_attrs = {SfOutHandleAttr_HipcCopy},
                           .out_handles = handle_out, );
}

HidSharedMemoryEntry::HidSharedMemoryEntry(::Service *hid_service, u64 aruid, u64 processId, u64 programId)
    : m_process_id(processId), m_program_id(programId), m_view(ViewForProgram(programId))
{
    Handle sharedMemHandle;

    m_status = _HidCreateClientAppletResource(hid_service, aruid, processId, &m_appletresource);
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

/*
    Everything Nintendo signs lives under 0x01, and applications start at 0x0100000000010000.
    A program id outside 0x01 is a homebrew sysmodule (sys-con itself is 0x69..., the overlay
    loader 0x42...), and below the application range sit the system modules and applets -
    qlaunch, am, overlayDisp, the Controllers applet - none of which takes motion or rumble
    from a pad, and all of which are best served by the real hid itself.
*/
bool HidSharedMemoryManager::ShouldMitm(u64 program_id)
{
    if (program_id == AlbumProgramId)
        return true;

    return program_id >= ApplicationProgramIdStart && program_id <= ApplicationProgramIdEnd;
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

static controllerlib::RumbleActuator ToRumbleActuator(const HidVibrationValue &value)
{
    return controllerlib::RumbleActuator{
        .amp_low = value.amp_low,
        .freq_low = value.freq_low,
        .amp_high = value.amp_high,
        .freq_high = value.freq_high,
    };
}

controllerlib::RumbleValue HidSharedMemoryManager::GetRumble(uint8_t player_idx) const
{
    return controllerlib::RumbleValue{
        .left = ToRumbleActuator(GetVibration(player_idx, 0)),
        .right = ToRumbleActuator(GetVibration(player_idx, 1)),
    };
}

void HidSharedMemoryManager::ClearVibration(uint8_t player_idx)
{
    const controllerlib::RumbleActuator idle{};
    for (uint8_t device_idx = 0; device_idx < HidSharedMemoryController::VibrationDeviceCount; device_idx++)
        SetVibration(player_idx, device_idx, HidVibrationValue{.amp_low = 0.0f, .freq_low = idle.freq_low, .amp_high = 0.0f, .freq_high = idle.freq_high});
}

std::shared_ptr<HidSharedMemoryController> HidSharedMemoryManager::AttachControllerAt(uint8_t player_idx, bool has_motion)
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

    m_controller_list[player_idx] = std::make_shared<HidSharedMemoryController>(player_idx, has_motion);
    ClearVibration(player_idx);
    m_player_owned[player_idx].store(true, std::memory_order_relaxed);

    {
        std::lock_guard<std::recursive_mutex> shmem_lock(m_mutex_sharedmemory);
        m_controller_list[player_idx]->TakeOverSixAxis();
    }

    ::syscon::logger::LogDebug("HidSharedMemoryManager attached a controller on player %d", player_idx + 1);
    return m_controller_list[player_idx];
}

/*
    Once the controller is gone the mirror copies the slot's six-axis lifo from the real hid
    again, whole, on its next tick.
*/
void HidSharedMemoryManager::DetachController(std::shared_ptr<HidSharedMemoryController> controller)
{
    int player_idx = -1;

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex_controller);

        for (size_t i = 0; i < m_controller_list.size(); i++)
        {
            if (m_controller_list[i] != controller)
                continue;

            m_player_owned[i].store(false, std::memory_order_relaxed);
            ClearVibration(i);
            m_controller_list[i] = nullptr;
            player_idx = (int)i;
            break;
        }
    }

    ::syscon::logger::LogDebug("HidSharedMemoryManager detached the controller of player %d", player_idx + 1);
}

std::shared_ptr<HidSharedMemoryEntry> HidSharedMemoryManager::CreateIfNotExists(::Service *hid_service, u64 aruid, u64 processId, u64 programId)
{
    /*
        Reclaim before allocating, not after. Every mitm'd process costs an applet resource
        session and a mapping of its real shared memory, so without this the list only grows.
        Running it from Add() would be too late: the allocation that needs the room happens
        in the constructor below.
    */
    RunGarbageCollector();

    std::shared_ptr<HidSharedMemoryEntry> entry = std::make_shared<HidSharedMemoryEntry>(hid_service, aruid, processId, programId);

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
            already reading it a jump in every lifo. The seed carries the real hid's six-axis
            samples for our slots, which are not numbered like ours.
        */
        if (FindEntry(entry->GetView()) == nullptr)
        {
            MirrorCopy(entry->GetFakeAddr(), entry->GetRealAddr(), HID_SHARED_MEMORY_SIZE);

            for (const auto &controller : m_controller_list)
            {
                if (controller != nullptr)
                    controller->EmptySixAxis(entry->GetView());
            }
        }

        m_sharedmemory_entry_list.push_back(entry);
    }

    DumpProcessesAndMemoryAddr();

    return 0;
}

void HidSharedMemoryManager::RunGarbageCollector()
{
    ::syscon::logger::LogDebug("HidSharedMemoryManager Garbage Collector running...");

    /*
        The kernel's process list, not pm:dmnt. pm:dmnt takes a single session, and at boot
        opening it from here - on the MITM thread, with a client waiting for its
        CreateAppletResource reply - sometimes never returned, and every hid client behind it
        hung.
    */
    u64 live_pids[0x80];
    s32 live_count = 0;
    Result rc = svcGetProcessList(&live_count, live_pids, static_cast<s32>(sizeof(live_pids) / sizeof(live_pids[0])));
    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("HidSharedMemoryManager: svcGetProcessList failed: 0x%08X", rc);
        return;
    }

    /*
        The mirror thread wants m_mutex_sharedmemory every 5 ms, and LogWarning (an SD write,
        ~7 ms) is far too slow to do while holding it. So: snapshot under the lock, decide
        unlocked, then take it again just to erase.
    */
    std::vector<std::shared_ptr<HidSharedMemoryEntry>> snapshot;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex_sharedmemory);
        snapshot = m_sharedmemory_entry_list;
    }

    std::vector<std::shared_ptr<HidSharedMemoryEntry>> dead;
    for (const auto &entry : snapshot)
    {
        if (std::find(live_pids, live_pids + live_count, entry->GetProcessId()) != live_pids + live_count)
            continue;

        ::syscon::logger::LogWarning("HidSharedMemoryManager Process id 0x%016" PRIx64 " is not running anymore, remove it !", entry->GetProcessId());
        dead.push_back(entry);
    }

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
        unmaps a shared memory and closes an IAppletResource session, none of which should
        run with the mirror thread blocked behind it.
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

    MirrorCopy(ByteAddr(fake, AfterNpadOffset), ByteAddr(real, AfterNpadOffset), AfterNpadSize);

    for (size_t i = NpadEntryCount; i > 0; i--)
    {
        const size_t npad = i - 1;
        HidNpadSharedMemoryEntry *fake_npad = &fake->npad.entries[npad];
        const HidNpadSharedMemoryEntry *real_npad = &real->npad.entries[npad];

        // An npad the console never populated stays zeroed on both sides. Skipping those
        // is what keeps the mirror cheap enough to run at the 200 Hz hid itself ticks at.
        if (real_npad->internal_state.style_set == 0 && fake_npad->internal_state.style_set == 0)
            continue;

        const bool six_axis_is_ours = npad < m_controller_list.size() && m_controller_list[npad] != nullptr && m_controller_list[npad]->HasMotion();
        if (!six_axis_is_ours)
        {
            MirrorCopy(fake_npad, real_npad, sizeof(HidNpadSharedMemoryEntry));
            continue;
        }

        MirrorCopy(ByteAddr(fake_npad, AfterSixAxisOffset), ByteAddr(real_npad, AfterSixAxisOffset), sizeof(HidNpadSharedMemoryEntry) - AfterSixAxisOffset);
        MirrorCopy(fake_npad, real_npad, SixAxisOffset);
    }

    MirrorCopy(fake, real, NpadOffset);
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
                    controller->PublishSixAxis();
            }
        }

        s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();
        if (execution_time_us < POLLING_FREQUENCY_US)
            svcSleepThread((POLLING_FREQUENCY_US - execution_time_us) * 1000); // Convert to nanoseconds
    }
}

HidSharedMemoryController::HidSharedMemoryController(uint8_t player_idx, bool has_motion)
    : m_player_idx(player_idx),
      m_has_motion(has_motion)
{
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
    The lifo still holds the real hid's samples, numbered unlike ours, and libnx restarts a read
    for as long as two neighbouring samples are not numbered consecutively. An empty lifo is the
    one state every reader copes with.
*/
void HidSharedMemoryController::EmptySixAxis(HidFakeView view)
{
    HidNpadSixAxisSensorLifo *lifo = FakeSixAxisLifo(view, m_player_idx);
    if (!m_has_motion || lifo == nullptr)
        return;

    lifo->header.buffer_count = 17;
    __atomic_store_n(&lifo->header.count, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&lifo->header.tail, 0u, __ATOMIC_RELEASE);
}

/*
    Our numbering starts above the highest number any view has shown, so a reader waiting for a
    newer sample never waits for ours to catch up.
*/
void HidSharedMemoryController::TakeOverSixAxis()
{
    for (size_t view = 0; view < HidFakeViewCount; view++)
    {
        const HidNpadSixAxisSensorLifo *lifo = FakeSixAxisLifo(static_cast<HidFakeView>(view), m_player_idx);
        if (lifo != nullptr && lifo->header.count != 0 && lifo->header.tail < lifo->header.buffer_count)
            m_sampling_number = std::max(m_sampling_number, lifo->storage[lifo->header.tail].sampling_number + 1);
    }

    for (size_t view = 0; view < HidFakeViewCount; view++)
        EmptySixAxis(static_cast<HidFakeView>(view));
}

/*
    Every tick, moving or not: a real controller keeps sampling at a fixed rate and the console
    treats a lifo that stops advancing as a sensor that went away.
*/
void HidSharedMemoryController::PublishSixAxis()
{
    if (!m_has_motion)
        return;

    m_motion.Step(m_angular_velocity, POLLING_FREQUENCY_US / 1e6f);

    HidSixAxisSensorState six_axis{};
    six_axis.delta_time = POLLING_FREQUENCY_US * 1000ULL;
    six_axis.sampling_number = m_sampling_number;
    six_axis.acceleration = m_acceleration;
    six_axis.angular_velocity = m_angular_velocity;
    six_axis.angle = m_motion.GetAngle();
    six_axis.direction = m_motion.GetDirection();
    six_axis.attributes = HidSixAxisSensorAttribute_IsConnected;

    for (size_t view = 0; view < HidFakeViewCount; view++)
    {
        HidNpadSixAxisSensorLifo *lifo = FakeSixAxisLifo(static_cast<HidFakeView>(view), m_player_idx);
        if (lifo != nullptr)
            AppendState(lifo, six_axis);
    }

    m_sampling_number++;
}

Result HidSharedMemoryController::Update(const SwitchPadState &state)
{
    std::lock_guard<std::recursive_mutex> lock(g_HidSharedMemoryManager.m_mutex_controller);

    m_acceleration = state.acceleration;
    m_angular_velocity = state.angular_velocity;

    return 0;
}

controllerlib::RumbleValue HidSharedMemoryController::GetRumble() const
{
    return g_HidSharedMemoryManager.GetRumble(m_player_idx);
}
