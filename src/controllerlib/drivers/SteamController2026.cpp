#include "drivers/SteamController2026.h"
#include <vector>
#include <chrono>

namespace controllerlib
{
    SteamController2026::SteamController2026(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger))
    {
        // m_interfaces is not populated until OpenInterfaces() runs, so the real count is
        // computed in Initialize(). Until then assume a single controller.
        m_controller_count = 1;

        for (int i = 0; i < STEAMCONTROLLER_MAX_INPUTS; i++)
            m_controllerInfo[i].m_is_connected = false;
    }

    SteamController2026::~SteamController2026()
    {
    }

    Status SteamController2026::Initialize()
    {
        Status result = BaseController::Initialize();
        if (result != Status::Success)
            return result;

        // Has to happen after BaseController::Initialize(), which is what calls OpenInterfaces()
        // and fills m_interfaces; in the constructor the vector is still empty.
        m_controller_count = (m_interfaces.size() > 1) ? STEAMCONTROLLER_MAX_INPUTS : 1;

        m_logger->Log(LogLevel::Debug, "SteamController2026[%04x-%04x] %d controller(s) detected on %d interface(s)", m_device->GetVendor(), m_device->GetProduct(), m_controller_count, (int)m_interfaces.size());

        return Status::Success;
    }

    Status SteamController2026::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        // buffer[0] is the report id; it must be present before we read it.
        if (size < 1)
            return Status::UnexpectedData;

        uint8_t report_id = buffer[0];

        // The Puck report 5 endpoints but the last seems to be the dongle.
        // The dongle is not a controller and should be ignored.
        if (*input_idx >= STEAMCONTROLLER_MAX_INPUTS)
            return Status::InvalidIndex;

        if (report_id == REPORT_INPUT || report_id == REPORT_INPUT_BLE)
        {
            Steam2026InputReport *controllerData = reinterpret_cast<Steam2026InputReport *>(buffer);
            if (size < sizeof(Steam2026InputReport))
            {
                m_logger->Log(LogLevel::Error, "SteamController2026[%04x-%04x] Unexpected data size (%d < %d)", m_device->GetVendor(), m_device->GetProduct(), size, sizeof(Steam2026InputReport));
                return Status::UnexpectedData;
            }

            m_rawInput.buttons[1] = controllerData->buttons.a;
            m_rawInput.buttons[2] = controllerData->buttons.b;
            m_rawInput.buttons[3] = controllerData->buttons.y;
            m_rawInput.buttons[4] = controllerData->buttons.x;
            m_rawInput.buttons[5] = controllerData->buttons.l1;
            m_rawInput.buttons[6] = controllerData->buttons.r1;
            m_rawInput.buttons[7] = controllerData->buttons.l2;
            m_rawInput.buttons[8] = controllerData->buttons.r2;
            m_rawInput.buttons[9] = controllerData->buttons.view;
            m_rawInput.buttons[10] = controllerData->buttons.menu;
            m_rawInput.buttons[11] = controllerData->buttons.quickaccess;
            m_rawInput.buttons[12] = controllerData->buttons.steam;
            m_rawInput.buttons[13] = controllerData->buttons.lstick;
            m_rawInput.buttons[14] = controllerData->buttons.rstick;

            m_rawInput.analog[AnalogAxis::X] = BaseController::Normalize(controllerData->left_stick_x, -32768, 32767);
            m_rawInput.analog[AnalogAxis::Y] = BaseController::Normalize(-controllerData->left_stick_y, -32768, 32767);
            m_rawInput.analog[AnalogAxis::Z] = BaseController::Normalize(controllerData->right_stick_x, -32768, 32767);
            m_rawInput.analog[AnalogAxis::Rz] = BaseController::Normalize(-controllerData->right_stick_y, -32768, 32767);

            m_rawInput.buttons[DPAD_UP_BUTTON_ID] = controllerData->buttons.dpad_up;
            m_rawInput.buttons[DPAD_RIGHT_BUTTON_ID] = controllerData->buttons.dpad_right;
            m_rawInput.buttons[DPAD_DOWN_BUTTON_ID] = controllerData->buttons.dpad_down;
            m_rawInput.buttons[DPAD_LEFT_BUTTON_ID] = controllerData->buttons.dpad_left;

            *rawData = m_rawInput;
            if (!m_controllerInfo[*input_idx].m_is_connected)
                OnControllerConnect(*input_idx);

            return Status::Success;
        }
        else if (report_id == REPORT_WIRELESS_STATUS_X || report_id == REPORT_WIRELESS_STATUS)
        {
            TritonWirelessStatus *statusData = reinterpret_cast<TritonWirelessStatus *>(buffer);
            bool was_connected = m_controllerInfo[*input_idx].m_is_connected;
            bool is_connected = statusData->state != 0x01;

            if (!was_connected && is_connected)
                OnControllerConnect(*input_idx);
            else if (was_connected && !is_connected)
                OnControllerDisconnect(*input_idx);
        }
        return Status::NothingTodo;
    }

    uint16_t SteamController2026::GetInputCount()
    {
        return m_controller_count;
    }

    bool SteamController2026::IsControllerConnected(uint16_t input_idx)
    {
        if (input_idx >= STEAMCONTROLLER_MAX_INPUTS)
            return false;

        return m_controllerInfo[input_idx].m_is_connected;
    }

    Status SteamController2026::OnControllerConnect(uint16_t input_idx)
    {
        m_logger->Log(LogLevel::Info, "SteamController2026 controller connected (Idx: %d) ...", input_idx);
        m_controllerInfo[input_idx].m_is_connected = true;
        return UpdateLizard(input_idx);
    }

    Status SteamController2026::OnControllerDisconnect(uint16_t input_idx)
    {
        m_logger->Log(LogLevel::Info, "SteamController2026 controller disconnected (Idx: %d) ...", input_idx);
        m_controllerInfo[input_idx].m_is_connected = false;
        return Status::Success;
    }

    Status SteamController2026::UpdateLizard(uint16_t input_idx)
    {
        if (!m_controllerInfo[input_idx].m_is_connected)
            return Status::NothingTodo;

        if (input_idx >= m_interfaces.size())
            return Status::NothingTodo;

        uint8_t buffer[HID_FEATURE_REPORT_BYTES] = {1};

        SetSettingsFeatureReportMsg *msg = reinterpret_cast<SetSettingsFeatureReportMsg *>(buffer + 1);

        msg->header.type = ID_SET_SETTINGS_VALUES;
        msg->setSettingsValues.settings[0].settingNum = SETTING_LIZARD_MODE;
        msg->setSettingsValues.settings[0].settingValue = LIZARD_MODE_OFF;
        msg->setSettingsValues.settings[1].settingNum = SETTING_STEAM_WATCHDOG_ENABLE;
        msg->setSettingsValues.settings[1].settingValue = WATCHDOG_DISABLE;
        msg->header.length = 2 * sizeof(ControllerSetting);

        return SendFeatureReport(input_idx, buffer, sizeof(buffer));
    }

    Status SteamController2026::SetRumble(uint16_t input_idx, float amp_high, float amp_low)
    {
        if (input_idx >= STEAMCONTROLLER_MAX_INPUTS || input_idx >= m_interfaces.size())
            return Status::InvalidIndex;

        if (!m_controllerInfo[input_idx].m_is_connected)
            return Status::NothingTodo;

        uint8_t buffer[HID_FEATURE_REPORT_BYTES] = {1};

        SimpleRumbleFeatureReportMsg *msg = reinterpret_cast<SimpleRumbleFeatureReportMsg *>(buffer + 1);

        // header.length stays 0, as SDL's Steam Deck driver sends it: the command is fixed size.
        msg->header.type = ID_TRIGGER_RUMBLE_CMD;
        msg->simpleRumble.rumbleType = RUMBLE_TYPE_DEFAULT;
        msg->simpleRumble.intensity = HAPTIC_INTENSITY_SYSTEM;
        msg->simpleRumble.leftMotorSpeed = (uint16_t)ScaleAmplitude(amp_low, 65535);
        msg->simpleRumble.rightMotorSpeed = (uint16_t)ScaleAmplitude(amp_high, 65535);
        msg->simpleRumble.leftGain = 2;
        msg->simpleRumble.rightGain = 0;

        return SendFeatureReport(input_idx, buffer, sizeof(buffer));
    }

    Status SteamController2026::SendFeatureReport(uint16_t input_idx, const uint8_t *buffer, uint16_t size)
    {
        return m_interfaces[input_idx]->ControlTransferOutput(
            0x21,
            0x09,
            (3 << 8) | buffer[0],
            m_interfaces[input_idx]->GetDescriptor()->bInterfaceNumber,
            buffer,
            size);
    }
} // namespace controllerlib
