#include "drivers/SteamController2026.h"

namespace controllerlib
{
    static_assert(sizeof(HapticRumbleOutputReport) == HID_RUMBLE_OUTPUT_REPORT_BYTES);

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

            // +/-2 g and +/-2000 dps full scale, the ranges SDL's steam_triton driver assumes.
            constexpr float AccelScale = 2.0f * StandardGravity / 32768.0f;
            constexpr float GyroScale = 2000.0f * RadiansPerDegree / 32768.0f;
            m_rawInput.motion.accel[0] = controllerData->imu.sAccelX * AccelScale;
            m_rawInput.motion.accel[1] = controllerData->imu.sAccelZ * AccelScale;
            m_rawInput.motion.accel[2] = -controllerData->imu.sAccelY * AccelScale;
            m_rawInput.motion.gyro[0] = controllerData->imu.sGyroX * GyroScale;
            m_rawInput.motion.gyro[1] = controllerData->imu.sGyroZ * GyroScale;
            m_rawInput.motion.gyro[2] = -controllerData->imu.sGyroY * GyroScale;

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

        // Otherwise the resend would hand the effect back to whoever connects into this slot.
        m_rumble[input_idx] = SteamControllerRumble{};

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

    Status SteamController2026::SetRumble(uint16_t input_idx, const RumbleValue &rumble)
    {
        if (input_idx >= STEAMCONTROLLER_MAX_INPUTS)
            return Status::InvalidIndex;

        m_rumble[input_idx].speed_low = static_cast<uint16_t>(ScaleAmplitude(rumble.LowAmplitude(), 65535));
        m_rumble[input_idx].speed_high = static_cast<uint16_t>(ScaleAmplitude(rumble.HighAmplitude(), 65535));

        return SendRumble(input_idx);
    }

    /*
        The motors stop by themselves shortly after the last haptic report, and the handler only
        calls SetRumble when the amplitude changes, so an effect a game holds steady has to be
        pushed again from the polling loop. Ref: SDL_hidapi_steam_triton.c.
    */
    Status SteamController2026::ReadInput(NormalizedButtonData *normalData, uint16_t *input_idx, uint32_t timeout_us)
    {
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

        for (uint16_t idx = 0; idx < STEAMCONTROLLER_MAX_INPUTS; idx++)
        {
            if (m_rumble[idx].speed_low == 0 && m_rumble[idx].speed_high == 0)
                continue;

            if ((now - m_rumble[idx].sent_at) < std::chrono::milliseconds(STEAMCONTROLLER_RUMBLE_RESEND_MS))
                continue;

            (void)SendRumble(idx);
        }

        return BaseController::ReadInput(normalData, input_idx, timeout_us);
    }

    Status SteamController2026::SendRumble(uint16_t input_idx)
    {
        if (m_outPipe.size() <= input_idx)
            return Status::InvalidIndex;

        if (!m_controllerInfo[input_idx].m_is_connected)
        {
            // Dropped rather than kept, so it cannot fire at whoever connects into this slot.
            m_rumble[input_idx] = SteamControllerRumble{};
            return Status::NothingTodo;
        }

        HapticRumbleOutputReport report{};
        report.report_id = ID_OUT_REPORT_HAPTIC_RUMBLE;
        report.hapticRumble.left.speed = m_rumble[input_idx].speed_low;
        report.hapticRumble.right.speed = m_rumble[input_idx].speed_high;

        m_rumble[input_idx].sent_at = std::chrono::steady_clock::now();

        return m_outPipe[input_idx]->Write(reinterpret_cast<const uint8_t *>(&report), sizeof(report));
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
