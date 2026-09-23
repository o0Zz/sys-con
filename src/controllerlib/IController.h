#pragma once
#include "IUSBDevice.h"
#include "ILogger.h"
#include "ControllerTypes.h"
#include "ControllerConfig.h"
#include "Status.h"

namespace controllerlib
{
    struct NormalizedStick
    {
        float axis_x{0.0f};
        float axis_y{0.0f};
    };

    // SDL's sensor convention, so scale constants carry over from SDL's hidapi drivers
    // unchanged: +X right, +Y up, +Z toward the player; accel in m/s^2 (the reaction to
    // gravity, so a pad lying flat reads +9.8 on Y), gyro in rad/s, counter-clockwise positive.
    struct NormalizedMotion
    {
        float accel[3]{};
        float gyro[3]{};
    };

    inline constexpr float StandardGravity = 9.80665f;
    inline constexpr float RadiansPerDegree = 3.14159265358979f / 180.0f;

    struct NormalizedButtonData
    {
        // Indexed by GamepadButton, not by pin (RawInputData::buttons is the pin-indexed one).
        GamepadButtonStates buttons{};
        NormalizedStick sticks[2]{};
        NormalizedMotion motion{};
    };

    class IController
    {
    protected:
        std::unique_ptr<IUSBDevice> m_device;
        ControllerConfig m_config;
        std::unique_ptr<ILogger> m_logger;

    public:
        IController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger) : m_device(std::move(device)),
                                                                                                                               m_config(config),
                                                                                                                               m_logger(std::move(logger))
        {
        }
        virtual ~IController() = default;

        virtual Status Initialize() = 0;
        virtual void Exit() = 0;

        virtual uint16_t GetInputCount() = 0;
        virtual Status ReadInput(NormalizedButtonData *normalData, uint16_t *input_idx, uint32_t timeout_us) = 0;

        virtual bool Support(ControllerFeature feature) const = 0;

        virtual Status SetRumble(uint16_t input_idx, float amp_high, float amp_low) = 0;

        virtual bool IsControllerConnected(uint16_t input_idx)
        {
            (void)input_idx;
            return true;
        }

        inline const ControllerConfig &GetConfig() const { return m_config; }

        inline IUSBDevice *GetDevice() { return m_device.get(); }
    };
} // namespace controllerlib
