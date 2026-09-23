#pragma once

#include <switch.h>

/*
    What a handler publishes for one pad. Motion is in Horizon's six-axis convention: +X
    right, +Y away from the player, +Z up; acceleration in G along gravity (a pad lying flat
    reads -1 on Z), angular velocity in revolutions per second.
*/
struct SwitchPadState
{
    u64 buttons;
    HidAnalogStickState analog_stick_l;
    HidAnalogStickState analog_stick_r;
    bool has_motion;
    HidVector acceleration;
    HidVector angular_velocity;
};
