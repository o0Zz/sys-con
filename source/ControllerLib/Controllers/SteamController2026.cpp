#include "Controllers/SteamController2026.h"
#include <vector>
#include <chrono>

SteamController2026::SteamController2026(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
    : BaseController(std::move(device), std::move(config), std::move(logger))
{
}

SteamController2026::~SteamController2026()
{
}

ControllerResult SteamController2026::Initialize()
{
    ControllerResult result = BaseController::Initialize();
    if (result != CONTROLLER_STATUS_SUCCESS)
        return result;

    return CONTROLLER_STATUS_SUCCESS;
}

ControllerResult SteamController2026::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
{
    uint8_t report_id = buffer[0];

    last_update = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (!last_lizard_update || (last_update - last_lizard_update) >= 3000)
    {
        uint8_t buffer[HID_FEATURE_REPORT_BYTES] = {1};

        SetSettingsFeatureReportMsg *msg =
            reinterpret_cast<SetSettingsFeatureReportMsg *>(buffer + 1);

        msg->header.type = ID_SET_SETTINGS_VALUES;
        msg->header.length = sizeof(ControllerSetting);

        msg->setSettingsValues.settings[0].settingNum =
            SETTING_LIZARD_MODE;

        msg->setSettingsValues.settings[0].settingValue =
            LIZARD_MODE_OFF;

        m_interfaces[0]->ControlTransferOutput(
            0x21,
            0x09,
            (3 << 8) | buffer[0],
            m_interfaces[0]->GetDescriptor()->bInterfaceNumber,
            buffer,
            sizeof(buffer));
        last_lizard_update = last_update;
    }

    if (report_id == REPORT_INPUT)
    {
        Steam2026InputReport *controllerData = reinterpret_cast<Steam2026InputReport *>(buffer);
        if (size < sizeof(Steam2026InputReport))
        {
            m_logger->Log(LogLevelError, "SteamController2026[%04x-%04x] Unexpected data size (%d < %d)", m_device->GetVendor(), m_device->GetProduct(), size, sizeof(Steam2026InputReport));
            return CONTROLLER_STATUS_UNEXPECTED_DATA;
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

        m_rawInput.analog[ControllerAnalogType_X] = BaseController::Normalize(controllerData->left_stick_x, -32768, 32767);
        m_rawInput.analog[ControllerAnalogType_Y] = BaseController::Normalize(-controllerData->left_stick_y, -32768, 32767);
        m_rawInput.analog[ControllerAnalogType_Z] = BaseController::Normalize(controllerData->right_stick_x, -32768, 32767);
        m_rawInput.analog[ControllerAnalogType_Rz] = BaseController::Normalize(-controllerData->right_stick_y, -32768, 32767);

        m_rawInput.buttons[DPAD_UP_BUTTON_ID] = controllerData->buttons.dpad_up;
        m_rawInput.buttons[DPAD_RIGHT_BUTTON_ID] = controllerData->buttons.dpad_right;
        m_rawInput.buttons[DPAD_DOWN_BUTTON_ID] = controllerData->buttons.dpad_down;
        m_rawInput.buttons[DPAD_LEFT_BUTTON_ID] = controllerData->buttons.dpad_left;

        *rawData = m_rawInput;
        disconnected[*input_idx] = false;
        return CONTROLLER_STATUS_SUCCESS;
    }
    else if (report_id == REPORT_WIRELESS_STATUS_X || report_id == REPORT_WIRELESS_STATUS)
    {
        TritonWirelessStatus_t *statusData = reinterpret_cast<TritonWirelessStatus_t *>(buffer);
        disconnected[*input_idx] = statusData->state == 0x01;
    }
    return CONTROLLER_STATUS_NOTHING_TODO;
}

bool SteamController2026::IsControllerConnected(uint16_t input_idx)
{
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return !(disconnected[input_idx] || (now - last_update) > 5000);
}