#include "hid_mitm_iappletresource.hpp"
// #include "hid_mitm_service.hpp"
// #include "hid_shared_memory.hpp"
#include <stratosphere.hpp>

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
        out.SetValue(m_fake_shared_memory.handle, true /*managed*/);
        R_SUCCEED();
    }

    void HidMitmAppletResource::InjectInputData(u64 button_mask, s32 left_stick_x, s32 left_stick_y, s32 right_stick_x, s32 right_stick_y)
    {
        /*if (!m_is_mapped || !m_shared_memory_ptr)
        {
            return;
        }

        u64 timestamp = GetCurrentTimestamp();

        NpadFullKeyState new_state = {};
        new_state.timestamp = timestamp;
        new_state.sampling_number = m_shared_memory_ptr->npad[0].full_key_lifo.header.count + 1;
        new_state.buttons = button_mask;
        new_state.analog_stick_l_x = left_stick_x;
        new_state.analog_stick_l_y = left_stick_y;
        new_state.analog_stick_r_x = right_stick_x;
        new_state.analog_stick_r_y = right_stick_y;
        new_state.attributes = 1; // Connected

        WriteToNpadLifo(m_shared_memory_ptr->npad[0].full_key_lifo, &new_state, sizeof(new_state));
        */
    }

    void HidMitmAppletResource::WriteToNpadLifo(NpadLifo &lifo, const void *state_data, size_t state_size)
    {
        /* Update header */
        u64 new_count = lifo.header.count + 1;
        u64 index = new_count % 17;

        /* Copy new state to LIFO buffer */
        std::memcpy(&lifo.full_key[index], state_data, state_size);

        /* Update header atomically */
        lifo.header.timestamp_last_entry = static_cast<const NpadFullKeyState *>(state_data)->timestamp;
        lifo.header.count = new_count;
        if (new_count >= 17)
        {
            lifo.header.timestamp_oldest_entry = lifo.full_key[(new_count + 1) % 17].timestamp;
        }
        lifo.header.max_count = 17;
    }

    u64 HidMitmAppletResource::GetCurrentTimestamp()
    {
        return ams::os::GetSystemTick().GetInt64Value();
    }
} // namespace ams::syscon::hid::mitm