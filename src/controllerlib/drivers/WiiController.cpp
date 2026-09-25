#include "drivers/WiiController.h"
#include <cstring>

#define STATE_EXTRA_POWER 0x04
#define STATE_NORMAL      0x10
#define STATE_WAVEBIRD    0x20

namespace controllerlib
{
    // Ref https://github.com/ToadKing/wii-u-gc-adapter/blob/master/wii-u-gc-adapter.c
    // Ref2 https://github.com/SternXD/dolphin/blob/master/Source/Core/InputCommon/GCAdapter.cpp
    // Reverse ING: https://gbatemp.net/threads/wii-u-gamecube-adapter-reverse-engineering-cont.388169/

    WiiController::WiiController(std::unique_ptr<IUSBDevice> &&device,
                                 const ControllerConfig &config,
                                 std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger))
    {
        for (int i = 0; i < WII_MAX_INPUTS; i++)
        {
            m_is_connected[i] = false;
            m_rumble_supported[i] = false;
        }
    }

    WiiController::~WiiController()
    {
    }

    Status WiiController::Initialize()
    {
        Status result = BaseController::Initialize();
        if (result != Status::Success)
            return result;

        if (m_outPipe.empty())
        {
            m_logger->Log(LogLevel::Error, "WiiController: Initialization failed, no output endpoint available");
            return Status::InvalidEndpoint;
        }

        /*
            Workaround needed for some adapter.
            Nyko and EVORETRO GameCube adapters or HS-N6420
            Ref: https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_gamecube.c
        */
        (void)m_interfaces[0]->ControlTransferOutput(0x21, 0x0B, 0x0001, 0x0000, nullptr, 0);

        /*
            Send initialization payload to the Wii controller (Needed for most of adapter)
        */
        const uint8_t init_payload[] = {0x13};
        (void)m_outPipe[0]->Write(init_payload, sizeof(init_payload));

        return Status::Success;
    }

    uint16_t WiiController::GetInputCount()
    {
        return WII_MAX_INPUTS;
    }

    Status WiiController::ReadNextBuffer(uint8_t *buffer, size_t *size, uint16_t *input_idx, uint32_t timeout_us)
    {
        Status result = Status::Success;
        if (m_current_wii_controller_idx == 0)
        {
            *size = WII_INPUT_BUFFER_SIZE;
            uint16_t input_idx_tmp = 0;

            result = BaseController::ReadNextBuffer(m_buffer, size, &input_idx_tmp, timeout_us);
            if (result != Status::Success)
                return result;

            if (m_buffer[0] != 0x21 || *size < WII_INPUT_BUFFER_SIZE)
                return Status::UnexpectedData;
        }

        memcpy(buffer, &m_buffer[1 + (m_current_wii_controller_idx * 9)], 9);
        *size = 9;

        *input_idx = m_current_wii_controller_idx;
        m_current_wii_controller_idx = (m_current_wii_controller_idx + 1) % WII_MAX_INPUTS;

        return Status::Success;
    }

    Status WiiController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        if (size < 9)
            return Status::UnexpectedData;

        /*
            input_idx selects which of the adapter's ports this slice came from, and is used to
            index m_is_connected / m_rumble_supported, so it must be range-checked first.
            (Same guard as SteamController2026, see #107.)
        */
        if (*input_idx >= WII_MAX_INPUTS)
            return Status::InvalidIndex;

        uint8_t status = buffer[0];

        // Rumble needs the adapter's extra power lead, and a WaveBird has no motor at all.
        m_rumble_supported[*input_idx] = ((status & STATE_EXTRA_POWER) != 0) && ((status & STATE_WAVEBIRD) == 0);
        m_is_connected[*input_idx] = (status & (STATE_NORMAL | STATE_WAVEBIRD)) != 0;

        if (!m_is_connected[*input_idx])
            return Status::NothingTodo;

        uint16_t btns = ((uint16_t)buffer[1]) | ((uint16_t)buffer[2] << 8);

        for (int j = 0; j < 16; j++)
        {
            uint16_t mask = (1 << j);
            rawData->buttons[j + 1] = (btns & mask) ? 1 : 0;
        }

        rawData->analog[AnalogAxis::X] = BaseController::Normalize(buffer[3], 0, 255);
        rawData->analog[AnalogAxis::Y] = BaseController::Normalize(buffer[4], 0, 255);
        rawData->analog[AnalogAxis::Rx] = BaseController::Normalize(buffer[5], 0, 255);
        rawData->analog[AnalogAxis::Ry] = BaseController::Normalize(buffer[6], 0, 255);
        rawData->analog[AnalogAxis::Z] = BaseController::Normalize(buffer[7], 0, 255);
        rawData->analog[AnalogAxis::Rz] = BaseController::Normalize(buffer[8], 0, 255);

        return Status::Success;
    }

    bool WiiController::IsControllerConnected(uint16_t input_idx)
    {
        if (input_idx >= WII_MAX_INPUTS)
            return false;

        return m_is_connected[input_idx];
    }

    Status WiiController::SetRumble(uint16_t input_idx, const RumbleValue &rumble)
    {
        if (input_idx >= WII_MAX_INPUTS)
            return Status::InvalidIndex;

        if (m_outPipe.empty())
            return Status::InvalidEndpoint;

        // The adapter only knows on and off, and the port needs the extra power lead for it.
        if (!m_rumble_supported[input_idx])
            return Status::NotImplemented;

        rumbleData[1 + input_idx] = rumble.IsActive() ? 1 : 0;

        return m_outPipe[0]->Write(rumbleData, sizeof(rumbleData));
    }
} // namespace controllerlib
