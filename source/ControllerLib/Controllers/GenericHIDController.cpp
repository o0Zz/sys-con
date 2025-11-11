#include "Controllers/GenericHIDController.h"
#include "HIDReportDescriptor.h"
#include "HIDJoystick.h"
#include <string.h>

#define USB_DT_REPORT              0x22
#define USB_REQUEST_GET_DESCRIPTOR 0x06
#define USB_REQUEST_SET_IDLE       0x0A

// https://www.usb.org/sites/default/files/documents/hid1_11.pdf  p55

GenericHIDController::GenericHIDController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
    : BaseController(std::move(device), config, std::move(logger)),
      m_joystick_count(0)
{
    Log(LogLevelDebug, "GenericHIDController[%04x-%04x] Created !", m_device->GetVendor(), m_device->GetProduct());
}

GenericHIDController::~GenericHIDController()
{
}

ControllerResult GenericHIDController::Initialize()
{
    ControllerResult result = BaseController::Initialize();
    if (result != CONTROLLER_STATUS_SUCCESS)
        return result;

    uint8_t buffer[CONTROLLER_HID_REPORT_BUFFER_SIZE];
    uint16_t size = sizeof(buffer);
    // https://www.usb.org/sites/default/files/hid1_11.pdf

    /// SET_IDLE
    result = m_interfaces[0]->ControlTransferOutput((uint8_t)IUSBEndpoint::USB_ENDPOINT_OUT | 0x20 | (uint8_t)USB_RECIPIENT_INTERFACE, USB_REQUEST_SET_IDLE, 0, m_interfaces[0]->GetDescriptor()->bInterfaceNumber, nullptr, 0);
    if (result != CONTROLLER_STATUS_SUCCESS)
        Log(LogLevelError, "GenericHIDController[%04x-%04x] SET_IDLE failed, continue anyway ...", m_device->GetVendor(), m_device->GetProduct());

    // Get HID report descriptor
    result = m_interfaces[0]->ControlTransferInput((uint8_t)IUSBEndpoint::USB_ENDPOINT_IN | (uint8_t)USB_RECIPIENT_INTERFACE, USB_REQUEST_GET_DESCRIPTOR, (USB_DT_REPORT << 8), m_interfaces[0]->GetDescriptor()->bInterfaceNumber, buffer, &size);
    if (result != CONTROLLER_STATUS_SUCCESS)
    {
        Log(LogLevelError, "GenericHIDController[%04x-%04x] Failed to get HID report descriptor", m_device->GetVendor(), m_device->GetProduct());
        return result;
    }

    Log(LogLevelTrace, "GenericHIDController[%04x-%04x] Got descriptor for interface %d", m_device->GetVendor(), m_device->GetProduct(), m_interfaces[0]->GetDescriptor()->bInterfaceNumber);
    LogBuffer(LogLevelTrace, buffer, size);

    Log(LogLevelDebug, "GenericHIDController[%04x-%04x] Parsing descriptor ...", m_device->GetVendor(), m_device->GetProduct());
    std::shared_ptr<HIDReportDescriptor> descriptor = std::make_shared<HIDReportDescriptor>(buffer, size);

    Log(LogLevelDebug, "GenericHIDController[%04x-%04x] Looking for joystick/gamepad profile ...", m_device->GetVendor(), m_device->GetProduct());
    m_joystick = std::make_shared<HIDJoystick>(descriptor);
    m_joystick_count = m_joystick->getCount();

    if (m_joystick_count == 0)
    {
        Log(LogLevelError, "GenericHIDController[%04x-%04x] HID report descriptor don't contains joystick/gamepad", m_device->GetVendor(), m_device->GetProduct());
        return CONTROLLER_STATUS_HID_IS_NOT_JOYSTICK;
    }

    Log(LogLevelInfo, "GenericHIDController[%04x-%04x] USB joystick successfully opened (%d inputs detected) !", m_device->GetVendor(), m_device->GetProduct(), GetInputCount());

    return CONTROLLER_STATUS_SUCCESS;
}

uint16_t GenericHIDController::GetInputCount()
{
    return std::min((int)m_joystick_count, CONTROLLER_MAX_INPUTS);
}

ControllerResult GenericHIDController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
{
    HIDJoystickData joystick_data;

    if (!m_joystick->parseData(buffer, (uint16_t)size, &joystick_data))
    {
        Log(LogLevelError, "GenericHIDController[%04x-%04x] Failed to parse input data (size=%d)", m_device->GetVendor(), m_device->GetProduct(), size);
        return CONTROLLER_STATUS_UNEXPECTED_DATA;
    }

    if (joystick_data.index >= GetInputCount())
    {
        Log(LogLevelError, "GenericHIDController[%04x-%04x] Unexpected input index %d/%d", m_device->GetVendor(), m_device->GetProduct(), joystick_data.index, GetInputCount());
        return CONTROLLER_STATUS_UNEXPECTED_DATA;
    }

    /*
         Special case for generic HID, input_idx might be bigger than 0 in case of multiple interfaces.
         If this is the case we expect to have 1 input per interface, thus we don't want to overwrite the input index.
    */
    if (input_idx != NULL && *input_idx == 0)
        *input_idx = joystick_data.index;

    for (int i = 0; i < MAX_CONTROLLER_BUTTONS; i++)
        rawData->buttons[i] = joystick_data.buttons[i];

    rawData->analog[ControllerAnalogType_Rx] = BaseController::Normalize(joystick_data.Rx, -32768, 32767);
    rawData->analog[ControllerAnalogType_Ry] = BaseController::Normalize(joystick_data.Ry, -32768, 32767);
    rawData->analog[ControllerAnalogType_X] = BaseController::Normalize(joystick_data.X, -32768, 32767);
    rawData->analog[ControllerAnalogType_Y] = BaseController::Normalize(joystick_data.Y, -32768, 32767);
    rawData->analog[ControllerAnalogType_Z] = BaseController::Normalize(joystick_data.Z, -32768, 32767);
    rawData->analog[ControllerAnalogType_Rz] = BaseController::Normalize(joystick_data.Rz, -32768, 32767);
    rawData->analog[ControllerAnalogType_Slider] = BaseController::Normalize(joystick_data.Slider, -32768, 32767);
    rawData->analog[ControllerAnalogType_Dial] = BaseController::Normalize(joystick_data.Dial, -32768, 32767);
    rawData->analog[ControllerAnalogType_Brake] = BaseController::Normalize(joystick_data.Brake, -32768, 32767);
    rawData->analog[ControllerAnalogType_Accelerator] = BaseController::Normalize(joystick_data.Accelerator, -32768, 32767);

    rawData->buttons[DPAD_UP_BUTTON_ID] = joystick_data.hat_switch == HIDJoystickHatSwitch::UP || joystick_data.hat_switch == HIDJoystickHatSwitch::UP_RIGHT || joystick_data.hat_switch == HIDJoystickHatSwitch::UP_LEFT;
    rawData->buttons[DPAD_RIGHT_BUTTON_ID] = joystick_data.hat_switch == HIDJoystickHatSwitch::RIGHT || joystick_data.hat_switch == HIDJoystickHatSwitch::UP_RIGHT || joystick_data.hat_switch == HIDJoystickHatSwitch::DOWN_RIGHT;
    rawData->buttons[DPAD_DOWN_BUTTON_ID] = joystick_data.hat_switch == HIDJoystickHatSwitch::DOWN || joystick_data.hat_switch == HIDJoystickHatSwitch::DOWN_RIGHT || joystick_data.hat_switch == HIDJoystickHatSwitch::DOWN_LEFT;
    rawData->buttons[DPAD_LEFT_BUTTON_ID] = joystick_data.hat_switch == HIDJoystickHatSwitch::LEFT || joystick_data.hat_switch == HIDJoystickHatSwitch::UP_LEFT || joystick_data.hat_switch == HIDJoystickHatSwitch::DOWN_LEFT;

    return CONTROLLER_STATUS_SUCCESS;
}
