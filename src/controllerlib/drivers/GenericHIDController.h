#pragma once

#include "BaseController.h"
#include <memory>

// HIDJoystick belongs to HIDDataInterpreter and lives in the global namespace. The
// forward declaration has to stay out here: inside `controllerlib` it would declare a
// second, unrelated type that shadows the real one at every use.
class HIDJoystick;

namespace controllerlib
{
    class GenericHIDController : public BaseController
    {
    private:
        std::shared_ptr<HIDJoystick> m_joystick;
        uint8_t m_joystick_count = 0;

    public:
        GenericHIDController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
        virtual ~GenericHIDController() override;

        virtual Status Initialize() override;

        virtual uint16_t GetInputCount() override;

        virtual Status ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;
    };
} // namespace controllerlib
