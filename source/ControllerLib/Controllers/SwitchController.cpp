#include "Controllers/SwitchController.h"

#define SWITCH_INPUT_BUFFER_SIZE 64

static_assert(SWITCH_INPUT_BUFFER_SIZE == 64, "Input byte for switch as to be 64 bytes long");

SwitchController::SwitchController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
    : BaseController(std::move(device), config, std::move(logger))
{
    cal_left_x.max = cal_left_y.max = cal_right_x.max = cal_right_y.max = 3400;
    cal_left_x.min = cal_left_y.min = cal_right_x.min = cal_right_y.min = 600;
}

SwitchController::~SwitchController()
{
}

ControllerResult SwitchController::Initialize()
{
    ControllerResult result = BaseController::Initialize();
    if (result != CONTROLLER_STATUS_SUCCESS)
        return result;

    if (m_outPipe.empty())
    {
        m_logger->Log(LogLevelError, "SwitchController: Initialization not complete ! No output endpoint found !");
        return CONTROLLER_STATUS_INVALID_ENDPOINT;
    }

    // Flush the input buffer
    uint8_t buffer[SWITCH_INPUT_BUFFER_SIZE]{0x00};
    do
    {
        size_t size = sizeof(buffer);
        result = m_inPipe[0]->Read(buffer, &size, 100 * 1000 /*timeout_us*/);
    } while (result == CONTROLLER_STATUS_SUCCESS);

    // Send the initialization packet
    uint8_t initPacket1[SWITCH_INPUT_BUFFER_SIZE]{0x80, 0x02};
    (void)m_outPipe[0]->Write(initPacket1, sizeof(initPacket1));

    // Read the response
    size_t size = sizeof(buffer);
    (void)m_inPipe[0]->Read(buffer, &size, 500 * 1000 /*timeout_us*/);

    // Forces the Joy-Con or Pro Controller to only talk over USB HID without any timeouts.
    uint8_t initPacket2_ForceToUSB[SWITCH_INPUT_BUFFER_SIZE]{0x80, 0x04};
    (void)m_outPipe[0]->Write(initPacket2_ForceToUSB, sizeof(initPacket2_ForceToUSB));

    return CONTROLLER_STATUS_SUCCESS;
}

ControllerResult SwitchController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
{
    (void)input_idx;
    SwitchButtonData *buttonData = reinterpret_cast<SwitchButtonData *>(buffer);

    if (size < sizeof(SwitchButtonData))
        return CONTROLLER_STATUS_UNEXPECTED_DATA;

    if (buttonData->report_id != 0x30)
        return CONTROLLER_STATUS_NOTHING_TODO;

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
    rawData->buttons[12] = buttonData->button12;
    rawData->buttons[13] = buttonData->button13;
    rawData->buttons[14] = buttonData->button14;
    rawData->buttons[15] = buttonData->button15;
    rawData->buttons[16] = buttonData->button16;
    rawData->buttons[17] = buttonData->button17;
    rawData->buttons[18] = buttonData->button18;
    rawData->buttons[19] = buttonData->button19;

    uint16_t left_x = BaseController::ReadBitsLE(buttonData->stick_left, 0, 12);
    uint16_t left_y = BaseController::ReadBitsLE(buttonData->stick_left, 12, 12);
    uint16_t right_x = BaseController::ReadBitsLE(buttonData->stick_right, 0, 12);
    uint16_t right_y = BaseController::ReadBitsLE(buttonData->stick_right, 12, 12);

    cal_left_x.max = std::max(left_x, cal_left_x.max);
    cal_left_x.min = std::min(left_x, cal_left_x.min);
    cal_left_y.max = std::max(left_y, cal_left_y.max);
    cal_left_y.min = std::min(left_y, cal_left_y.min);
    cal_right_x.max = std::max(right_x, cal_right_x.max);
    cal_right_x.min = std::min(right_x, cal_right_x.min);
    cal_right_y.max = std::max(right_y, cal_right_y.max);
    cal_right_y.min = std::min(right_y, cal_right_y.min);

    m_logger->Log(LogLevelTrace, "X=%u, Y=%u, Z=%u, Rz=%u (Calib: X=[%u,%u], Y=[%u,%u], Z=[%u,%u], Rz=[%u,%u])",
                  left_x, left_y, right_x, right_y,
                  cal_left_x.min, cal_left_x.max, cal_left_y.min, cal_left_y.max,
                  cal_right_x.min, cal_right_x.max, cal_right_y.min, cal_right_y.max);

    rawData->analog[ControllerAnalogType_X] = BaseController::Normalize(left_x, cal_left_x.min, cal_left_x.max, 2000);
    rawData->analog[ControllerAnalogType_Y] = -1.0f * BaseController::Normalize(left_y, cal_left_y.min, cal_left_y.max, 2000);
    rawData->analog[ControllerAnalogType_Z] = BaseController::Normalize(right_x, cal_right_x.min, cal_right_x.max, 2000);
    rawData->analog[ControllerAnalogType_Rz] = -1.0f * BaseController::Normalize(right_y, cal_right_y.min, cal_right_y.max, 2000);

    rawData->buttons[DPAD_UP_BUTTON_ID] = buttonData->dpad_up;
    rawData->buttons[DPAD_RIGHT_BUTTON_ID] = buttonData->dpad_right;
    rawData->buttons[DPAD_DOWN_BUTTON_ID] = buttonData->dpad_down;
    rawData->buttons[DPAD_LEFT_BUTTON_ID] = buttonData->dpad_left;

    return CONTROLLER_STATUS_SUCCESS;
}

bool SwitchController::Support(ControllerFeature feature)
{
    (void)feature;
    return false;
}

size_t SwitchController::GetMaxInputBufferSize()
{
    return SWITCH_INPUT_BUFFER_SIZE;
}
