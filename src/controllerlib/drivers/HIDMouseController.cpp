#include "drivers/HIDMouseController.h"
#include "drivers/HIDProtocol.h"
#include "HIDReportDescriptor.h"
#include "HIDMouse.h"

namespace controllerlib
{
    namespace
    {
        constexpr uint8_t BootInterfaceSubClass = 1;

        // HIDMouseData::buttons is indexed by HID button usage number, so index 0 is unused.
        constexpr struct
        {
            uint8_t usage;
            uint8_t bit;
        } MouseButtons[] = {
            {1, MOUSE_BUTTON_LEFT},
            {2, MOUSE_BUTTON_RIGHT},
            {3, MOUSE_BUTTON_MIDDLE},
            {4, MOUSE_BUTTON_BACK},
            {5, MOUSE_BUTTON_FORWARD},
        };
    } // namespace

    HIDMouseController::HIDMouseController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : IMouse(std::move(device), config, std::move(logger))
    {
        m_logger->Log(LogLevel::Debug, "HIDMouseController[%04x-%04x] Created !", m_device->GetVendor(), m_device->GetProduct());
    }

    HIDMouseController::~HIDMouseController()
    {
    }

    Status HIDMouseController::Initialize()
    {
        Status result = OpenPipes(m_device.get(), GetConfig(), m_logger.get());
        if (result != Status::Success)
            return result;

        IUSBInterface *interface = m_interfaces[0];

        if (interface->GetDescriptor()->bInterfaceSubClass == BootInterfaceSubClass)
        {
            result = hid::SetProtocol(interface, hid::PROTOCOL_REPORT);
            if (result != Status::Success)
            {
                m_logger->Log(LogLevel::Error, "HIDMouseController[%04x-%04x] SET_PROTOCOL(report) failed", m_device->GetVendor(), m_device->GetProduct());
                return Status::HidProtocolFailed;
            }
        }

        if (hid::SetIdle(interface, 0, 0) != Status::Success)
            m_logger->Log(LogLevel::Error, "HIDMouseController[%04x-%04x] SET_IDLE failed, continue anyway ...", m_device->GetVendor(), m_device->GetProduct());

        uint8_t buffer[CONTROLLER_HID_REPORT_BUFFER_SIZE];
        uint16_t size = sizeof(buffer);

        result = hid::GetReportDescriptor(interface, buffer, &size);
        if (result != Status::Success)
        {
            m_logger->Log(LogLevel::Error, "HIDMouseController[%04x-%04x] Failed to get HID report descriptor", m_device->GetVendor(), m_device->GetProduct());
            return result;
        }

        m_logger->LogBuffer(LogLevel::Trace, buffer, size);

        m_descriptor = std::make_shared<HIDReportDescriptor>(buffer, size);
        m_mouse = std::make_shared<HIDMouse>(m_descriptor);

        if (m_mouse->get_count() == 0)
        {
            m_logger->Log(LogLevel::Error, "HIDMouseController[%04x-%04x] HID report descriptor don't contains a mouse", m_device->GetVendor(), m_device->GetProduct());
            return Status::HidIsNotMouse;
        }

        m_logger->Log(LogLevel::Info, "HIDMouseController[%04x-%04x] USB mouse successfully opened !", m_device->GetVendor(), m_device->GetProduct());
        return Status::Success;
    }

    void HIDMouseController::Exit()
    {
        ClosePipes(m_device.get());
    }

    Status HIDMouseController::Accumulate(uint8_t *buffer, size_t size, MouseState *state)
    {
        HIDMouseData mouse_data;
        if (!m_mouse->parse_data(buffer, (uint16_t)size, &mouse_data))
        {
            m_logger->Log(LogLevel::Error, "HIDMouseController[%04x-%04x] Failed to parse input data (size=%d)", m_device->GetVendor(), m_device->GetProduct(), size);
            return Status::UnexpectedData;
        }

        state->deltaX += mouse_data.x;
        state->deltaY += mouse_data.y;
        state->deltaWheel += mouse_data.wheel;

        state->buttons = 0;
        for (const auto &entry : MouseButtons)
        {
            if (entry.usage < mouse_data.button_count && mouse_data.buttons[entry.usage])
                state->buttons |= entry.bit;
        }

        return Status::Success;
    }

    Status HIDMouseController::ReadInput(MouseState *state, uint32_t timeout_us)
    {
        uint8_t input_bytes[CONTROLLER_INPUT_BUFFER_SIZE];
        size_t size = sizeof(input_bytes);

        Status result = ReadEndpointOnce(0, input_bytes, &size, timeout_us);
        if (result != Status::Success)
            return result;

        *state = MouseState{};

        result = Accumulate(input_bytes, size, state);
        if (result != Status::Success)
            return result;

        /*
            Drain whatever else is already queued and sum it in. A mouse reports a delta, not
            a position, so keeping only the freshest report would silently delete motion --
            the opposite of the gamepad policy, where only the latest state matters.
        */
        for (;;)
        {
            size = sizeof(input_bytes);
            if (ReadEndpointOnce(0, input_bytes, &size, 0) != Status::Success)
                break;

            if (Accumulate(input_bytes, size, state) != Status::Success)
                break;
        }

        const int32_t factor = GetConfig().mouseSensitivityPercent;
        state->deltaX = (state->deltaX * factor) / 100;
        state->deltaY = (state->deltaY * factor) / 100;

        return Status::Success;
    }
} // namespace controllerlib
