#pragma once

#include <switch.h>

// Integrates angular velocity (rev/s) into the angle and orientation hid publishes next to it.
class SwitchMotion
{
public:
    SwitchMotion();

    void Step(const HidVector &angular_velocity, float dt_s);

    const HidVector &GetAngle() const { return m_angle; }
    const HidDirectionState &GetDirection() const { return m_direction; }

private:
    HidVector m_angle;
    HidDirectionState m_direction;
};
