#include "Controllers/WiiController.h"

#define STATE_EXTRA_POWER 0x04
#define STATE_NORMAL      0x10
#define STATE_WAVEBIRD    0x20

// Ref https://github.com/ToadKing/wii-u-gc-adapter/blob/master/wii-u-gc-adapter.c

WiiController::WiiController(std::unique_ptr<IUSBDevice> &&device,
                             const ControllerConfig &config,
                             std::unique_ptr<ILogger> &&logger)
    : BaseController(std::move(device), config, std::move(logger))
{
    for (int i = 0; i < WII_MAX_INPUTS; i++)
        m_is_connected[i] = false;
}

WiiController::~WiiController()
{
}

uint16_t WiiController::GetInputCount()
{
    return WII_MAX_INPUTS;
}

ControllerResult WiiController::ReadNextBuffer(uint8_t *buffer, size_t *size, uint16_t *input_idx, uint32_t timeout_us)
{
    ControllerResult result = CONTROLLER_STATUS_SUCCESS;
    if (m_current_wii_controller_idx == 0)
    {
        *size = WII_INPUT_BUFFER_SIZE;
        uint16_t input_idx_tmp = 0;

        result = BaseController::ReadNextBuffer(m_buffer, size, &input_idx_tmp, timeout_us);
        if (result != CONTROLLER_STATUS_SUCCESS)
            return result;

        if (m_buffer[0] != 0x21 || *size < WII_INPUT_BUFFER_SIZE)
            return CONTROLLER_STATUS_UNEXPECTED_DATA;
    }

    memcpy(buffer, &m_buffer[1 + (m_current_wii_controller_idx * 9)], 9);
    *size = 9;

    *input_idx = m_current_wii_controller_idx;
    m_current_wii_controller_idx = (m_current_wii_controller_idx + 1) % WII_MAX_INPUTS;

    return CONTROLLER_STATUS_SUCCESS;
}

ControllerResult WiiController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
{
    if (size < 9)
        return CONTROLLER_STATUS_UNEXPECTED_DATA;

    uint8_t status = buffer[0];

    m_rumble_supported[*input_idx] = ((status & STATE_EXTRA_POWER) != 0); // Rumble can be supported if Extra Power bit is set
    m_is_connected[*input_idx] = (status & (STATE_NORMAL | STATE_WAVEBIRD)) != 0;

    if (!m_is_connected[*input_idx])
        return CONTROLLER_STATUS_NOTHING_TODO;

    uint16_t btns = ((uint16_t)buffer[1]) | ((uint16_t)buffer[2] << 8);

    for (int j = 0; j < 16; j++)
    {
        uint16_t mask = (1 << j);
        rawData->buttons[j + 1] = (btns & mask) ? 1 : 0;
    }

    rawData->analog[ControllerAnalogType_X] = BaseController::Normalize(buffer[3], 0, 255);
    rawData->analog[ControllerAnalogType_Y] = BaseController::Normalize(buffer[4], 0, 255);
    rawData->analog[ControllerAnalogType_Rx] = BaseController::Normalize(buffer[5], 0, 255);
    rawData->analog[ControllerAnalogType_Ry] = BaseController::Normalize(buffer[6], 0, 255);
    rawData->analog[ControllerAnalogType_Z] = BaseController::Normalize(buffer[7], 0, 255);
    rawData->analog[ControllerAnalogType_Rz] = BaseController::Normalize(buffer[8], 0, 255);

    return CONTROLLER_STATUS_SUCCESS;
}

bool WiiController::IsControllerConnected(uint16_t input_idx)
{
    return m_is_connected[input_idx];
}

bool WiiController::Support(ControllerFeature feature)
{
    if (feature == SUPPORTS_RUMBLE)
        return true;

    return false;
}
