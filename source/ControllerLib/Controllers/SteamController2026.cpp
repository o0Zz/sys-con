#include "Controllers/SteamController2026.h"
#include <vector>

#define REPORT_INPUT 0x45

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
    (void)input_idx;
    Steam2026InputReport *controllerData = reinterpret_cast<Steam2026InputReport *>(buffer);

    if (controllerData->report_id == REPORT_INPUT)
    {
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

        if (controllerData->seq_num > 0xA0) { // SDL3 runs this every 3 seconds, but this should work well enough
            for (auto &&interface : m_interfaces)
            {
                // Send feature report to exit Lizard mode, otherwise some inputs will cause issues
                char8_t buffer[HID_FEATURE_REPORT_BYTES] = { 1 };
        
                FeatureReportMsg *msg =
                    reinterpret_cast<FeatureReportMsg*>(buffer + 1);
        
                msg->header.type = ID_SET_SETTINGS_VALUES;
                msg->header.length = sizeof(ControllerSetting);
        
                msg->payload.setSettingsValues.settings[0].settingNum =
                    SETTING_LIZARD_MODE;
        
                msg->payload.setSettingsValues.settings[0].settingValue =
                    LIZARD_MODE_OFF;
        
                ControllerResult lizardResult =
                    interface->ControlTransferOutput(
                        0x21,
                        0x09,
                        (3 << 8) | buffer[0],
                        interface->GetDescriptor()->bInterfaceNumber,
                        buffer,
                        sizeof(buffer)
                    );

                if (lizardResult != CONTROLLER_STATUS_SUCCESS)
                    return lizardResult;
            }
        }
        return CONTROLLER_STATUS_SUCCESS;
    }
    else
        m_logger->Log(LogLevelDebug, "SteamController2026[%04x-%04x] sent Report ID %d with data size %d", m_device->GetVendor(), m_device->GetProduct(), controllerData->report_id, size);

    return CONTROLLER_STATUS_NOTHING_TODO;
}

// Below doesn't work yet, just copy-pasted from Xbox code. I don't think it matters yet since I believe rumble isn't fully implemented
bool SteamController2026::Support(ControllerFeature feature)
{
    if (feature == SUPPORTS_RUMBLE)
        return true;

    return false;
}

ControllerResult SteamController2026::SetRumble(uint16_t input_idx, float amp_high, float amp_low)
{
    (void)input_idx;
    const uint8_t rumble_data[]{
        0x09, 0x00, 0x00,
        0x09, 0x00, 0x0f, 0x00, 0x00,
        (uint8_t)(amp_high * 255),
        (uint8_t)(amp_low * 255),
        0xff, 0x00, 0x00};

    if (m_outPipe.size() <= input_idx)
        return CONTROLLER_STATUS_INVALID_INDEX;

    return m_outPipe[input_idx]->Write(rumble_data, sizeof(rumble_data));
}