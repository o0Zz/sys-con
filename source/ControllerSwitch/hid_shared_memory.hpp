#pragma once
#include <vapours.hpp>

namespace ams::syscon::hid::mitm
{

    struct CommonLifoHeader
    {
        u64 timestamp_last_entry;
        u64 count;
        u64 timestamp_oldest_entry;
        u64 max_count;
    };

    struct NpadFullKeyState
    {
        u64 timestamp;
        u64 sampling_number;
        u64 buttons;
        s32 analog_stick_l_x;
        s32 analog_stick_l_y;
        s32 analog_stick_r_x;
        s32 analog_stick_r_y;
        u32 attributes;
        u32 reserved;
    };

    struct NpadHandheldState
    {
        u64 timestamp;
        u64 sampling_number;
        u64 buttons;
        s32 analog_stick_l_x;
        s32 analog_stick_l_y;
        s32 analog_stick_r_x;
        s32 analog_stick_r_y;
        u32 attributes;
        u32 reserved;
    };

    struct NpadLifo
    {
        CommonLifoHeader header;
        union
        {
            NpadFullKeyState full_key[17];
            NpadHandheldState handheld[17];
        };
    };

    struct NpadInternalState
    {
        u32 style_set;
        u32 joy_assignment_mode;
        u32 full_key_color;
        u32 joycon_color;
        NpadLifo full_key_lifo;
        NpadLifo handheld_lifo;
        u32 device_type;
        u32 reserved;
        u64 system_properties;
        u32 system_button_properties;
        u32 battery_level[3];
        u32 nfc_xcd_device_handle;
        u64 nfc_xcd_device_handle_state;
        u64 gc_controller_color;
        u64 supported_npad_style_set;
        u8 reserved2[0x20];
        u32 applet_footer_ui_attributes;
        u8 applet_footer_ui_type;
        u8 reserved3[0x5B];
        u64 mutex;
        u8 padding[0x4000]; // Pad to 0x5000 bytes
    };

    // static_assert(sizeof(NpadInternalState) == 0x5000);

    struct HidSharedMemory
    {
        u8 header[0x400];
        u8 touchscreen[0x3000];
        u8 mouse[0x400];
        u8 keyboard[0x400];
        u8 reserved1[0x400];
        u32 npad_count;
        u8 reserved2[0x4];
        NpadInternalState npad[10];
        u8 reserved3[0x1000];
        u8 gesture[0x800];
        u8 console_six_axis_sensor[0x400];
        u8 debug_pad[0x400];
        u8 reserved4[0x2000];
    };

    // static_assert(sizeof(HidSharedMemory) == 0x40000);

    enum class NpadButton : u64
    {
        A = BIT(0),
        B = BIT(1),
        X = BIT(2),
        Y = BIT(3),
        StickL = BIT(4),
        StickR = BIT(5),
        L = BIT(6),
        R = BIT(7),
        ZL = BIT(8),
        ZR = BIT(9),
        Plus = BIT(10),
        Minus = BIT(11),
        Left = BIT(12),
        Up = BIT(13),
        Right = BIT(14),
        Down = BIT(15),
        StickLLeft = BIT(16),
        StickLUp = BIT(17),
        StickLRight = BIT(18),
        StickLDown = BIT(19),
        StickRLeft = BIT(20),
        StickRUp = BIT(21),
        StickRRight = BIT(22),
        StickRDown = BIT(23),
        LeftSL = BIT(24),
        LeftSR = BIT(25),
        RightSL = BIT(26),
        RightSR = BIT(27),
        HomeButton = BIT(28),
        Capture = BIT(29),
    };

} // namespace ams::syscon::hid::mitm
