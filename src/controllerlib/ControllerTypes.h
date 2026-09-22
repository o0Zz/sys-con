#pragma once

#include <stdint.h>

#define CONTROLLER_MAX_INPUTS             4
#define CONTROLLER_INPUT_BUFFER_SIZE      256
#define CONTROLLER_HID_REPORT_BUFFER_SIZE 512

namespace controllerlib
{
    // What a driver can do beyond reporting buttons and axes; extend as drivers grow
    // (accelerometer, gyro, ...). Queried through IController::Support().
    enum ControllerFeature : uint8_t
    {
        SUPPORTS_RUMBLE,
    };
} // namespace controllerlib
