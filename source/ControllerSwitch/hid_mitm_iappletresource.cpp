#include "hid_mitm_iappletresource.hpp"
#include "SwitchLogger.h"
#include <stratosphere.hpp>

extern "C" Mutex shmem_mutex;
// PID, Original first, fake second
static std::unordered_map<u64, std::pair<HidSharedMemory *, HidSharedMemory *>> sharedmems;

void add_shmem(u64 pid, SharedMemory *real_shmem, SharedMemory *fake_shmem)
{
    ::syscon::logger::LogDebug("add_shmem...");

    mutexLock(&shmem_mutex);
    HidSharedMemory *real_mapped = (HidSharedMemory *)shmemGetAddr(real_shmem);
    HidSharedMemory *fake_mapped = (HidSharedMemory *)shmemGetAddr(fake_shmem);
    sharedmems[pid] = std::pair<HidSharedMemory *, HidSharedMemory *>(real_mapped, fake_mapped);
    mutexUnlock(&shmem_mutex);
}

void del_shmem(u64 pid)
{
    ::syscon::logger::LogDebug("del_shmem...");

    mutexLock(&shmem_mutex);
    if (sharedmems.find(pid) != sharedmems.end())
    {
        sharedmems.erase(pid);
    }
    mutexUnlock(&shmem_mutex);
}
namespace ams::syscon::hid::mitm
{
    HidMitmAppletResource::HidMitmAppletResource()
    {
    }

    HidMitmAppletResource::~HidMitmAppletResource()
    {
    }

    Result HidMitmAppletResource::GetSharedMemoryHandle(ams::sf::OutCopyHandle out)
    {
        ::syscon::logger::LogDebug("HidMitmAppletResource::GetSharedMemoryHandle...");
        out.SetValue(m_fake_shared_memory.handle, true /*managed*/);
        R_SUCCEED();
    }

    int HidMitmAppletResource::apply_fake_gamepad(HidSharedMemory *tmp_shmem_mem)
    {
        // Looking for a free controller
        int gamepad;

        for (gamepad = 0; gamepad < 8; gamepad++) // 8player
        {
            if (tmp_shmem_mem->npad[gamepad].device_type == 0)
                break;
        }

        ::syscon::logger::LogDebug("apply_fake_gamepad: %d...", gamepad);

        // Pro controller magic
        // tmp_shmem_mem->npad[gamepad].deviceType = 0x01;

        tmp_shmem_mem->npad[gamepad].full_key_color = 0x0000FFFF; // Blue
        tmp_shmem_mem->npad[gamepad].joycon_color = 0xFF0000FF;   // Red

        tmp_shmem_mem->npad[gamepad].full_key_lifo.header.timestamp_last_entry = GetCurrentTimestamp();
        tmp_shmem_mem->npad[gamepad].full_key_lifo.header.count = 0;
        tmp_shmem_mem->npad[gamepad].full_key_lifo.header.timestamp_oldest_entry = 0;
        tmp_shmem_mem->npad[gamepad].full_key_lifo.header.max_count = 17;

        tmp_shmem_mem->npad[gamepad].full_key_lifo.full_key[0].timestamp = GetCurrentTimestamp();
        tmp_shmem_mem->npad[gamepad].full_key_lifo.full_key[0].sampling_number = 0;
        tmp_shmem_mem->npad[gamepad].full_key_lifo.full_key[0].buttons = 0x00;           // No buttons pressed
        tmp_shmem_mem->npad[gamepad].full_key_lifo.full_key[0].analog_stick_l_x = 32000; // Centered
        tmp_shmem_mem->npad[gamepad].full_key_lifo.full_key[0].analog_stick_l_y = 0;     // Centered
        tmp_shmem_mem->npad[gamepad].full_key_lifo.full_key[0].analog_stick_r_x = 0;     // Centered
        tmp_shmem_mem->npad[gamepad].full_key_lifo.full_key[0].analog_stick_r_y = 0;     // Centered
        tmp_shmem_mem->npad[gamepad].full_key_lifo.full_key[0].attributes = 0;           // No attributes
        tmp_shmem_mem->npad[gamepad].full_key_lifo.full_key[0].reserved = 0;             // Reserved

        return 0;
    }

    u64 HidMitmAppletResource::GetCurrentTimestamp()
    {
        return ams::os::GetSystemTick().GetInt64Value();
    }
} // namespace ams::syscon::hid::mitm