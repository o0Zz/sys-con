#include "SwitchMotion.h"
#include <cmath>

namespace
{
    constexpr float RadiansPerRevolution = 2.0f * 3.14159265358979f;

    void Normalize(float v[3])
    {
        const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        for (int i = 0; i < 3; i++)
            v[i] /= length;
    }
} // namespace

SwitchMotion::SwitchMotion()
    : m_angle{},
      m_direction{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}
{
}

/*
    The rows of the direction matrix are the pad's own axes in the world frame, so a small
    body-frame rotation phi = w * dt turns it into (I - [phi]x) * M. The step is first order,
    so the rows are re-orthonormalized every time to keep float error from accumulating.
*/
void SwitchMotion::Step(const HidVector &angular_velocity, float dt_s)
{
    m_angle.x += angular_velocity.x * dt_s;
    m_angle.y += angular_velocity.y * dt_s;
    m_angle.z += angular_velocity.z * dt_s;

    const float x = angular_velocity.x * RadiansPerRevolution * dt_s;
    const float y = angular_velocity.y * RadiansPerRevolution * dt_s;
    const float z = angular_velocity.z * RadiansPerRevolution * dt_s;

    float(*m)[3] = m_direction.direction;
    float r[3][3];
    for (int i = 0; i < 3; i++)
    {
        r[0][i] = m[0][i] + z * m[1][i] - y * m[2][i];
        r[1][i] = m[1][i] - z * m[0][i] + x * m[2][i];
    }

    Normalize(r[0]);
    const float dot = r[0][0] * r[1][0] + r[0][1] * r[1][1] + r[0][2] * r[1][2];
    for (int i = 0; i < 3; i++)
        r[1][i] -= dot * r[0][i];
    Normalize(r[1]);

    for (int i = 0; i < 3; i++)
    {
        m[0][i] = r[0][i];
        m[1][i] = r[1][i];
    }
    m[2][0] = m[0][1] * m[1][2] - m[0][2] * m[1][1];
    m[2][1] = m[0][2] * m[1][0] - m[0][0] * m[1][2];
    m[2][2] = m[0][0] * m[1][1] - m[0][1] * m[1][0];
}
