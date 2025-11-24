#include "Controllers/Xbox360WirelessController.h"

// https://www.partsnotincluded.com/understanding-the-xbox-360-wired-controllers-usb-data/

static constexpr uint8_t reconnectPacket[]{0x08, 0x00, 0x0F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static constexpr uint8_t poweroffPacket[]{0x00, 0x00, 0x08, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static constexpr uint8_t initDriverPacket[]{0x00, 0x00, 0x02, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

Xbox360WirelessController::Xbox360WirelessController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
    : BaseController(std::move(device), config, std::move(logger))
{
    for (int i = 0; i < XBOX360_MAX_INPUTS; i++)
        m_is_connected[i] = false;
}

Xbox360WirelessController::~Xbox360WirelessController()
{
}

ControllerResult Xbox360WirelessController::OpenInterfaces()
{
    ControllerResult result = BaseController::OpenInterfaces();
    if (result != CONTROLLER_STATUS_SUCCESS)
        return result;

    if (m_inPipe.size() < XBOX360_MAX_INPUTS)
    {
        m_logger->Log(LogLevelError, "Xbox360WirelessController: Not enough input endpoints (%d / %d)", m_inPipe.size(), XBOX360_MAX_INPUTS);
        return CONTROLLER_STATUS_INVALID_ENDPOINT;
    }

    return CONTROLLER_STATUS_SUCCESS;
}

void Xbox360WirelessController::CloseInterfaces()
{
    for (int i = 0; i < XBOX360_MAX_INPUTS; i++)
    {
        if (m_is_connected[i])
            OnControllerDisconnect(i);
    }

    BaseController::CloseInterfaces();
}

ControllerResult Xbox360WirelessController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
{
    // https://github.com/xboxdrv/xboxdrv/blob/stable/src/xbox360_controller.cpp
    // https://github.com/felis/USB_Host_Shield_2.0/blob/master/XBOXRECV.cpp

    if (buffer[0] & 0x08) // Connect/Disconnect
    {
        bool is_connected = (buffer[1] & 0x80) != 0;

        if (m_is_connected[*input_idx] != is_connected)
        {
            if (is_connected)
                OnControllerConnect(*input_idx);
            else
                OnControllerDisconnect(*input_idx);
        }

        return CONTROLLER_STATUS_NOTHING_TODO;
    }

    if (buffer[0] == 0x00 && buffer[1] == 0x01 && buffer[2] == 0x00 && buffer[3] == 0xf0) // Controller Data
    {
        Xbox360ButtonData *buttonData = reinterpret_cast<Xbox360ButtonData *>(buffer + 4);
        if (size < sizeof(Xbox360ButtonData))
            return CONTROLLER_STATUS_UNEXPECTED_DATA;

        if (buttonData->type == XBOX360INPUT_BUTTON) // Button data
        {
            rawData->buttons[1] = buttonData->button1;
            rawData->buttons[2] = buttonData->button2;
            rawData->buttons[3] = buttonData->button3;
            rawData->buttons[4] = buttonData->button4;
            rawData->buttons[5] = buttonData->button5;
            rawData->buttons[6] = buttonData->button6;
            rawData->buttons[7] = buttonData->button7;
            rawData->buttons[8] = buttonData->button8;
            rawData->buttons[9] = buttonData->button9;
            rawData->buttons[10] = buttonData->button10;
            rawData->buttons[11] = buttonData->button11;

            rawData->analog[ControllerAnalogType_Rx] = BaseController::Normalize(buttonData->Rx, 0, 255);
            rawData->analog[ControllerAnalogType_Ry] = BaseController::Normalize(buttonData->Ry, 0, 255);
            rawData->analog[ControllerAnalogType_X] = BaseController::Normalize(buttonData->X, -32768, 32767);
            rawData->analog[ControllerAnalogType_Y] = BaseController::Normalize(-buttonData->Y, -32768, 32767);
            rawData->analog[ControllerAnalogType_Z] = BaseController::Normalize(buttonData->Z, -32768, 32767);
            rawData->analog[ControllerAnalogType_Rz] = BaseController::Normalize(-buttonData->Rz, -32768, 32767);

            rawData->buttons[DPAD_UP_BUTTON_ID] = buttonData->dpad_up;
            rawData->buttons[DPAD_RIGHT_BUTTON_ID] = buttonData->dpad_right;
            rawData->buttons[DPAD_DOWN_BUTTON_ID] = buttonData->dpad_down;
            rawData->buttons[DPAD_LEFT_BUTTON_ID] = buttonData->dpad_left;

            return CONTROLLER_STATUS_SUCCESS;
        }
    }

    return CONTROLLER_STATUS_NOTHING_TODO;
}

bool Xbox360WirelessController::Support(ControllerFeature feature)
{
    if (feature == SUPPORTS_RUMBLE)
        return true;

    return false;
}

uint16_t Xbox360WirelessController::GetInputCount()
{
    return XBOX360_MAX_INPUTS;
}

ControllerResult Xbox360WirelessController::SetRumble(uint16_t input_idx, float amp_high, float amp_low)
{
    uint8_t rumbleData[]{0x00, (uint8_t)(input_idx + 1), 0x0F, 0xC0, 0x00, (uint8_t)(amp_high * 255), (uint8_t)(amp_low * 255), 0x00, 0x00, 0x00, 0x00, 0x00};
    if (m_outPipe.size() <= input_idx)
        return CONTROLLER_STATUS_INVALID_INDEX;

    return m_outPipe[input_idx]->Write(rumbleData, sizeof(rumbleData));
}

bool Xbox360WirelessController::IsControllerConnected(uint16_t input_idx)
{
    return m_is_connected[input_idx];
}

ControllerResult Xbox360WirelessController::SetLED(uint16_t input_idx, Xbox360LEDValue value)
{
    uint8_t ledPacket[]{0x00, (uint8_t)(input_idx + 1), 0x08, (uint8_t)(value | 0x40), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    if (m_outPipe.size() <= input_idx)
        return CONTROLLER_STATUS_SUCCESS;

    return m_outPipe[input_idx]->Write(ledPacket, sizeof(ledPacket));
}

ControllerResult Xbox360WirelessController::OnControllerConnect(uint16_t input_idx)
{
    m_logger->Log(LogLevelInfo, "Xbox360WirelessController Wireless controller connected (Idx: %d) ...", input_idx);

    if (m_outPipe.size() > input_idx)
    {
        m_outPipe[input_idx]->Write(reconnectPacket, sizeof(reconnectPacket));
        m_outPipe[input_idx]->Write(initDriverPacket, sizeof(initDriverPacket));
    }

    SetLED(input_idx, (Xbox360LEDValue)((int)XBOX360LED_TOPLEFT + input_idx));

    m_is_connected[input_idx] = true;

    return CONTROLLER_STATUS_SUCCESS;
}

ControllerResult Xbox360WirelessController::OnControllerDisconnect(uint16_t input_idx)
{
    m_logger->Log(LogLevelInfo, "Xbox360WirelessController Wireless controller disconnected (Idx: %d) ...", input_idx);

    if (m_outPipe.size() > input_idx)
        m_outPipe[input_idx]->Write(poweroffPacket, sizeof(poweroffPacket));

    m_is_connected[input_idx] = false;

    return CONTROLLER_STATUS_SUCCESS;
}
