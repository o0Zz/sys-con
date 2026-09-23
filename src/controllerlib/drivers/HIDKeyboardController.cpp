#include "drivers/HIDKeyboardController.h"
#include "drivers/HIDProtocol.h"
#include "HIDReportDescriptor.h"
#include "HIDKeyboard.h"

#include <algorithm>

namespace controllerlib
{
    namespace
    {
        constexpr uint8_t BootInterfaceSubClass = 1;

        constexpr struct
        {
            uint8_t key;
            uint8_t lock;
            uint8_t led;
        } LockKeys[] = {
            {(uint8_t)HIDKeyboardKey::CapsLock, KEYBOARD_LOCK_CAPS, hid::LED_CAPS_LOCK},
            {(uint8_t)HIDKeyboardKey::NumLock, KEYBOARD_LOCK_NUM, hid::LED_NUM_LOCK},
            {(uint8_t)HIDKeyboardKey::ScrollLock, KEYBOARD_LOCK_SCROLL, hid::LED_SCROLL_LOCK},
        };
    } // namespace

    HIDKeyboardController::HIDKeyboardController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : IKeyboard(std::move(device), config, std::move(logger))
    {
        m_logger->Log(LogLevel::Debug, "HIDKeyboardController[%04x-%04x] Created !", m_device->GetVendor(), m_device->GetProduct());
    }

    HIDKeyboardController::~HIDKeyboardController()
    {
    }

    Status HIDKeyboardController::Initialize()
    {
        Status result = OpenPipes(m_device.get(), GetConfig(), m_logger.get());
        if (result != Status::Success)
            return result;

        IUSBInterface *interface = m_interfaces[0];

        /*
            A boot-capable device powers up in report protocol, so this is normally a no-op --
            but it makes the wire format ours rather than whatever the previous host left
            behind. On a non-boot interface the request is undefined and devices STALL it.
        */
        if (interface->GetDescriptor()->bInterfaceSubClass == BootInterfaceSubClass)
        {
            result = hid::SetProtocol(interface, hid::PROTOCOL_REPORT);
            if (result != Status::Success)
            {
                m_logger->Log(LogLevel::Error, "HIDKeyboardController[%04x-%04x] SET_PROTOCOL(report) failed", m_device->GetVendor(), m_device->GetProduct());
                return Status::HidProtocolFailed;
            }
        }

        // Idle 0 means "report only on change", which is what a keyboard should do.
        if (hid::SetIdle(interface, 0, 0) != Status::Success)
            m_logger->Log(LogLevel::Error, "HIDKeyboardController[%04x-%04x] SET_IDLE failed, continue anyway ...", m_device->GetVendor(), m_device->GetProduct());

        uint8_t buffer[CONTROLLER_HID_REPORT_BUFFER_SIZE];
        uint16_t size = sizeof(buffer);

        result = hid::GetReportDescriptor(interface, buffer, &size);
        if (result != Status::Success)
        {
            m_logger->Log(LogLevel::Error, "HIDKeyboardController[%04x-%04x] Failed to get HID report descriptor", m_device->GetVendor(), m_device->GetProduct());
            return result;
        }

        m_logger->LogBuffer(LogLevel::Trace, buffer, size);

        m_descriptor = std::make_shared<HIDReportDescriptor>(buffer, size);
        m_keyboard = std::make_shared<HIDKeyboard>(m_descriptor);

        if (m_keyboard->get_count() == 0)
        {
            m_logger->Log(LogLevel::Error, "HIDKeyboardController[%04x-%04x] HID report descriptor don't contains a keyboard", m_device->GetVendor(), m_device->GetProduct());
            return Status::HidIsNotKeyboard;
        }

        m_logger->Log(LogLevel::Info, "HIDKeyboardController[%04x-%04x] USB keyboard successfully opened !", m_device->GetVendor(), m_device->GetProduct());
        return Status::Success;
    }

    void HIDKeyboardController::Exit()
    {
        ClosePipes(m_device.get());
    }

    bool HIDKeyboardController::WasHeld(uint8_t key) const
    {
        return std::find(m_previous_keys.begin(), m_previous_keys.end(), key) != m_previous_keys.end();
    }

    void HIDKeyboardController::UpdateLocks(const KeyboardState &state)
    {
        const uint8_t before = m_locks;
        uint8_t leds = 0;

        for (const auto &entry : LockKeys)
        {
            const bool held = std::find(state.keys.begin(), state.keys.begin() + state.keyCount, entry.key) != state.keys.begin() + state.keyCount;
            if (held && !WasHeld(entry.key))
                m_locks ^= entry.lock;

            if (m_locks & entry.lock)
                leds |= entry.led;
        }

        if (m_locks == before)
            return;

        if (hid::SetOutputReport(m_interfaces[0], 0, &leds, sizeof(leds)) != Status::Success)
            m_logger->Log(LogLevel::Debug, "HIDKeyboardController[%04x-%04x] Failed to set lock LEDs", m_device->GetVendor(), m_device->GetProduct());
    }

    Status HIDKeyboardController::ReadInput(KeyboardState *state, uint32_t timeout_us)
    {
        uint8_t input_bytes[CONTROLLER_INPUT_BUFFER_SIZE];
        size_t size = sizeof(input_bytes);

        Status result = ReadEndpointOnce(0, input_bytes, &size, timeout_us);
        if (result != Status::Success)
            return result;

        HIDKeyboardData keyboard_data;
        if (!m_keyboard->parse_data(input_bytes, (uint16_t)size, &keyboard_data))
        {
            m_logger->Log(LogLevel::Error, "HIDKeyboardController[%04x-%04x] Failed to parse input data (size=%d)", m_device->GetVendor(), m_device->GetProduct(), size);
            return Status::UnexpectedData;
        }

        /*
            Rollover means the device cannot say which keys are down, not that they came up.
            Publishing it would release everything the user is still holding, so drop the
            report and leave the last state standing.
        */
        if (keyboard_data.key_count > 0 && keyboard_data.keys[0] == HIDKeyboardKey::ErrorRollOver)
            return Status::NothingTodo;

        state->modifiers = keyboard_data.modifiers;
        state->keyCount = std::min<uint8_t>(keyboard_data.key_count, KeyboardMaxKeys);
        state->keys = {};
        for (uint8_t i = 0; i < state->keyCount; i++)
            state->keys[i] = (uint8_t)keyboard_data.keys[i];

        UpdateLocks(*state);
        state->locks = m_locks;

        m_previous_keys = state->keys;
        return Status::Success;
    }
} // namespace controllerlib
