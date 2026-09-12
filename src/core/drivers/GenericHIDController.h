#pragma once

#include "BaseController.h"
#include <memory>

class HIDJoystick;

class GenericHIDController : public BaseController
{
private:
    std::shared_ptr<HIDJoystick> m_joystick;
    uint8_t m_joystick_count = 0;

public:
    GenericHIDController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
    virtual ~GenericHIDController() override;

    virtual ControllerResult Initialize() override;

    virtual uint16_t GetInputCount() override;

    virtual ControllerResult ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;
};