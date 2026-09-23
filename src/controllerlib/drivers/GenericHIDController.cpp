#include "drivers/GenericHIDController.h"
#include "drivers/HIDProtocol.h"
#include "HIDReportDescriptor.h"
#include "HIDJoystick.h"
#include <string.h>

namespace controllerlib
{
    GenericHIDController::GenericHIDController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger)),
          m_joystick_count(0)
    {
        m_logger->Log(LogLevel::Debug, "GenericHIDController[%04x-%04x] Created !", m_device->GetVendor(), m_device->GetProduct());
    }

    GenericHIDController::~GenericHIDController()
    {
    }

    Status GenericHIDController::Initialize()
    {
        Status result = BaseController::Initialize();
        if (result != Status::Success)
            return result;

        uint8_t buffer[CONTROLLER_HID_REPORT_BUFFER_SIZE];
        uint16_t size = sizeof(buffer);
        // https://www.usb.org/sites/default/files/hid1_11.pdf

        result = hid::SetIdle(m_interfaces[0], 0, 0);
        if (result != Status::Success)
            m_logger->Log(LogLevel::Error, "GenericHIDController[%04x-%04x] SET_IDLE failed, continue anyway ...", m_device->GetVendor(), m_device->GetProduct());

        result = hid::GetReportDescriptor(m_interfaces[0], buffer, &size);
        if (result != Status::Success)
        {
            m_logger->Log(LogLevel::Error, "GenericHIDController[%04x-%04x] Failed to get HID report descriptor", m_device->GetVendor(), m_device->GetProduct());
            return result;
        }

        m_logger->Log(LogLevel::Trace, "GenericHIDController[%04x-%04x] Got descriptor for interface %d", m_device->GetVendor(), m_device->GetProduct(), m_interfaces[0]->GetDescriptor()->bInterfaceNumber);
        m_logger->LogBuffer(LogLevel::Trace, buffer, size);

        m_logger->Log(LogLevel::Debug, "GenericHIDController[%04x-%04x] Parsing descriptor ...", m_device->GetVendor(), m_device->GetProduct());
        std::shared_ptr<HIDReportDescriptor> descriptor = std::make_shared<HIDReportDescriptor>(buffer, size);

        m_logger->Log(LogLevel::Debug, "GenericHIDController[%04x-%04x] Looking for joystick/gamepad profile ...", m_device->GetVendor(), m_device->GetProduct());
        m_joystick = std::make_shared<HIDJoystick>(descriptor);
        m_joystick_count = m_joystick->get_count();

        if (m_joystick_count == 0)
        {
            m_logger->Log(LogLevel::Error, "GenericHIDController[%04x-%04x] HID report descriptor don't contains joystick/gamepad", m_device->GetVendor(), m_device->GetProduct());
            return Status::HidIsNotJoystick;
        }

        m_logger->Log(LogLevel::Info, "GenericHIDController[%04x-%04x] USB joystick successfully opened (%d inputs detected) !", m_device->GetVendor(), m_device->GetProduct(), GetInputCount());
        return Status::Success;
    }

    uint16_t GenericHIDController::GetInputCount()
    {
        return std::min((int)m_joystick_count, CONTROLLER_MAX_INPUTS);
    }

    Status GenericHIDController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        HIDJoystickData joystick_data;

        if (!m_joystick->parse_data(buffer, (uint16_t)size, &joystick_data))
        {
            m_logger->Log(LogLevel::Error, "GenericHIDController[%04x-%04x] Failed to parse input data (size=%d)", m_device->GetVendor(), m_device->GetProduct(), size);
            return Status::UnexpectedData;
        }

        if (joystick_data.index >= GetInputCount())
        {
            m_logger->Log(LogLevel::Error, "GenericHIDController[%04x-%04x] Unexpected input index %d/%d", m_device->GetVendor(), m_device->GetProduct(), joystick_data.index, GetInputCount());
            return Status::UnexpectedData;
        }

        /*
             Special case for generic HID, input_idx might be bigger than 0 in case of multiple interfaces.
             If this is the case we expect to have 1 input per interface, thus we don't want to overwrite the input index.
        */
        if (input_idx != NULL && *input_idx == 0)
            *input_idx = joystick_data.index;

        for (int i = 0; i < joystick_data.button_count; i++)
            rawData->buttons[i] = joystick_data.buttons[i];

        rawData->analog[AnalogAxis::Rx] = BaseController::Normalize(joystick_data.rx, -32768, 32767);
        rawData->analog[AnalogAxis::Ry] = BaseController::Normalize(joystick_data.ry, -32768, 32767);
        rawData->analog[AnalogAxis::X] = BaseController::Normalize(joystick_data.x, -32768, 32767);
        rawData->analog[AnalogAxis::Y] = BaseController::Normalize(joystick_data.y, -32768, 32767);
        rawData->analog[AnalogAxis::Z] = BaseController::Normalize(joystick_data.z, -32768, 32767);
        rawData->analog[AnalogAxis::Rz] = BaseController::Normalize(joystick_data.rz, -32768, 32767);
        rawData->analog[AnalogAxis::Slider] = BaseController::Normalize(joystick_data.slider, -32768, 32767);
        rawData->analog[AnalogAxis::Dial] = BaseController::Normalize(joystick_data.dial, -32768, 32767);
        rawData->analog[AnalogAxis::Brake] = BaseController::Normalize(joystick_data.brake, -32768, 32767);
        rawData->analog[AnalogAxis::Accelerator] = BaseController::Normalize(joystick_data.accelerator, -32768, 32767);

        rawData->buttons[DPAD_UP_BUTTON_ID] = joystick_data.hat_switch == HIDJoystickHatSwitch::UP || joystick_data.hat_switch == HIDJoystickHatSwitch::UP_RIGHT || joystick_data.hat_switch == HIDJoystickHatSwitch::UP_LEFT;
        rawData->buttons[DPAD_RIGHT_BUTTON_ID] = joystick_data.hat_switch == HIDJoystickHatSwitch::RIGHT || joystick_data.hat_switch == HIDJoystickHatSwitch::UP_RIGHT || joystick_data.hat_switch == HIDJoystickHatSwitch::DOWN_RIGHT;
        rawData->buttons[DPAD_DOWN_BUTTON_ID] = joystick_data.hat_switch == HIDJoystickHatSwitch::DOWN || joystick_data.hat_switch == HIDJoystickHatSwitch::DOWN_RIGHT || joystick_data.hat_switch == HIDJoystickHatSwitch::DOWN_LEFT;
        rawData->buttons[DPAD_LEFT_BUTTON_ID] = joystick_data.hat_switch == HIDJoystickHatSwitch::LEFT || joystick_data.hat_switch == HIDJoystickHatSwitch::UP_LEFT || joystick_data.hat_switch == HIDJoystickHatSwitch::DOWN_LEFT;

        return Status::Success;
    }
} // namespace controllerlib
