#pragma once

#include <algorithm>
#include <stdint.h>

#define CONTROLLER_MAX_INPUTS             4
#define CONTROLLER_INPUT_BUFFER_SIZE      256
#define CONTROLLER_HID_REPORT_BUFFER_SIZE 512

namespace controllerlib
{
    // What a driver can do beyond reporting buttons and axes. Queried through IController::Support().
    enum ControllerFeature : uint8_t
    {
        SUPPORTS_RUMBLE,
        SUPPORTS_MOTION,
    };

    /*
        One actuator of a Switch pad: a low and a high frequency band, each with an amplitude in
        [0, 1] and a frequency in Hz. The defaults are the resonant frequencies hid itself uses
        for a silent actuator.
    */
    struct RumbleActuator
    {
        float amp_low = 0.0f;
        float freq_low = 160.0f;
        float amp_high = 0.0f;
        float freq_high = 320.0f;

        bool operator==(const RumbleActuator &) const = default;
    };

    /*
        What a game asked for, one actuator per grip. A driver with two eccentric motors has no
        frequency and no left/right split, so it takes the loudest amplitude of each band: the
        low band drives the heavy motor, the high band the light one.
    */
    struct RumbleValue
    {
        RumbleActuator left;
        RumbleActuator right;

        float LowAmplitude() const { return std::max(left.amp_low, right.amp_low); }
        float HighAmplitude() const { return std::max(left.amp_high, right.amp_high); }
        bool IsActive() const { return LowAmplitude() > 0.0f || HighAmplitude() > 0.0f; }

        bool operator==(const RumbleValue &) const = default;
    };
} // namespace controllerlib
