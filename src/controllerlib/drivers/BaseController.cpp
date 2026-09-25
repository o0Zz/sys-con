#include "drivers/BaseController.h"
#include <cmath>
#include <chrono>
#include <cstring>

namespace controllerlib
{
    // https://www.usb.org/sites/default/files/documents/hid1_11.pdf  p55

    BaseController::BaseController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : IController(std::move(device), config, std::move(logger))
    {
        m_logger->Log(LogLevel::Debug, "Controller[%04x-%04x] Created !", m_device->GetVendor(), m_device->GetProduct());
    }

    BaseController::~BaseController()
    {
    }

    Status BaseController::Initialize()
    {
        m_logger->Log(LogLevel::Debug, "Controller[%04x-%04x] Initializing ...", m_device->GetVendor(), m_device->GetProduct());

        Status result = OpenInterfaces();
        if (result != Status::Success)
        {
            m_logger->Log(LogLevel::Error, "Controller[%04x-%04x] Failed to open interfaces !", m_device->GetVendor(), m_device->GetProduct());
            return result;
        }

        return Status::Success;
    }

    void BaseController::Exit()
    {
        CloseInterfaces();
    }

    uint16_t BaseController::GetInputCount()
    {
        return 1;
    }

    size_t BaseController::GetMaxInputBufferSize()
    {
        return CONTROLLER_INPUT_BUFFER_SIZE;
    }

    Status BaseController::OpenInterfaces()
    {
        m_logger->Log(LogLevel::Debug, "Controller[%04x-%04x] Opening interfaces ...", m_device->GetVendor(), m_device->GetProduct());

        Status result = m_device->Open();
        if (result != Status::Success)
        {
            m_logger->Log(LogLevel::Error, "Controller[%04x-%04x] Failed to open device !", m_device->GetVendor(), m_device->GetProduct());
            return result;
        }

        std::vector<std::unique_ptr<IUSBInterface>> &interfaces = m_device->GetInterfaces();
        for (auto &&interface : interfaces)
        {
            m_logger->Log(LogLevel::Debug, "Controller[%04x-%04x] Opening interface %d/%d ...", m_device->GetVendor(), m_device->GetProduct(), m_interfaces.size() + 1, interfaces.size());

            Status interfaceResult = interface->Open();
            if (interfaceResult != Status::Success)
            {
                m_logger->Log(LogLevel::Error, "Controller[%04x-%04x] Failed to open interface !", m_device->GetVendor(), m_device->GetProduct());
                return interfaceResult;
            }

            for (uint8_t idx = 0; idx < 15; idx++)
            {
                IUSBEndpoint *inEndpoint = interface->GetEndpoint(IUSBEndpoint::USB_ENDPOINT_IN, idx);
                if (inEndpoint == NULL)
                    continue;

                Status endpointResult = inEndpoint->Open(GetConfig().inputMaxPacketSize);
                if (endpointResult != Status::Success)
                {
                    m_logger->Log(LogLevel::Error, "Controller[%04x-%04x] Failed to open input endpoint idx: %d !", m_device->GetVendor(), m_device->GetProduct(), idx);
                    return endpointResult;
                }

                m_inPipe.push_back(inEndpoint);
            }

            for (uint8_t idx = 0; idx < 15; idx++)
            {
                IUSBEndpoint *outEndpoint = interface->GetEndpoint(IUSBEndpoint::USB_ENDPOINT_OUT, idx);
                if (outEndpoint == NULL)
                    continue;

                Status endpointResult = outEndpoint->Open(GetConfig().outputMaxPacketSize);
                if (endpointResult != Status::Success)
                {
                    m_logger->Log(LogLevel::Error, "Controller[%04x-%04x] Failed to open output endpoint idx: %d !", m_device->GetVendor(), m_device->GetProduct(), idx);
                    return endpointResult;
                }

                m_outPipe.push_back(outEndpoint);
            }

            m_interfaces.push_back(interface.get());
        }

        if (m_inPipe.empty())
        {
            m_logger->Log(LogLevel::Error, "Controller[%04x-%04x] Not input endpoint found !", m_device->GetVendor(), m_device->GetProduct());
            return Status::InvalidEndpoint;
        }

        m_logger->Log(LogLevel::Debug, "Controller[%04x-%04x] successfully opened !", m_device->GetVendor(), m_device->GetProduct());
        return Status::Success;
    }

    void BaseController::CloseInterfaces()
    {
        m_device->Close();

        /*
            m_inPipe/m_outPipe/m_interfaces are non-owning pointers into the device we just
            closed. Drop them so a later ReadInput()/SetRumble() can't dereference endpoints
            that no longer exist; both now see empty pipe lists and fail cleanly instead.
        */
        m_inPipe.clear();
        m_outPipe.clear();
        m_interfaces.clear();
    }

    static void StoreAmplitude(uint8_t *packet, const ControllerRumbleField &field, float amplitude)
    {
        if (field.size == 0)
            return;

        const uint32_t value = BaseController::ScaleAmplitude(amplitude, (field.size == 1) ? 0xFF : 0xFFFF);

        for (uint8_t i = 0; i < field.size; i++)
        {
            const uint8_t shift = field.littleEndian ? (i * 8) : ((field.size - 1 - i) * 8);
            packet[field.offset + i] = (uint8_t)(value >> shift);
        }
    }

    Status BaseController::SetRumble(uint16_t input_idx, const RumbleValue &rumble)
    {
        const ControllerRumbleConfig &config = GetConfig().rumble;

        if (!config.IsValid())
            return Status::NotImplemented;

        if (m_outPipe.size() <= input_idx)
            return Status::InvalidIndex;

        uint8_t packet[MAX_RUMBLE_PACKET_SIZE];
        memcpy(packet, config.packet.data(), config.packetSize);

        StoreAmplitude(packet, config.low, rumble.LowAmplitude());
        StoreAmplitude(packet, config.high, rumble.HighAmplitude());

        return m_outPipe[input_idx]->Write(packet, config.packetSize);
    }

    Status BaseController::ReadEndpointLatest(uint16_t endpoint_idx, uint8_t *buffer, size_t *size, uint32_t timeout_us)
    {
        const size_t capacity = std::min((size_t)m_inPipe[endpoint_idx]->GetDescriptor()->wMaxPacketSize, *size);

        size_t latestSize = capacity;
        Status result = m_inPipe[endpoint_idx]->Read(buffer, &latestSize, timeout_us);
        if (result != Status::Success)
            return result;

        if (latestSize == 0)
            return Status::NothingTodo;

        /*
         Drain any further reports that are already queued, keeping only the freshest one.
         The device (especially wireless ones like the Steam Puck) can deliver reports faster
         than we poll and in bursts; without draining we would replay stale frames and fall
         progressively behind. For a gamepad only the latest state matters.
        */
        uint8_t drainBuffer[CONTROLLER_INPUT_BUFFER_SIZE];
        for (;;)
        {
            size_t drainSize = capacity;
            if (m_inPipe[endpoint_idx]->Read(drainBuffer, &drainSize, 0) != Status::Success || drainSize == 0)
                break;

            memcpy(buffer, drainBuffer, drainSize);
            latestSize = drainSize;
        }

        *size = latestSize;
        return Status::Success;
    }

    Status BaseController::ReadNextBuffer(uint8_t *buffer, size_t *size, uint16_t *input_idx, uint32_t timeout_us)
    {
        const size_t endpoint_count = m_inPipe.size();
        if (endpoint_count == 0)
            return Status::NothingTodo;

        const size_t requested_size = *size;

        /*
         Fast pass: probe every endpoint without blocking and service the first one that has data.
         A non-blocking read still posts (and keeps posted) the underlying async transfer, so this
         arms all endpoints concurrently - reports are then captured in parallel between sweeps
         instead of only on the single endpoint we happen to visit this tick. This is what keeps a
         multi-endpoint device (e.g. the Steam Puck, ~5 endpoints) sampled near its real report rate
         rather than at (rate / endpoint_count).
        */
        for (size_t i = 0; i < endpoint_count; i++)
        {
            uint16_t endpoint_idx = (m_current_controller_idx + i) % endpoint_count;

            size_t sz = requested_size;
            if (ReadEndpointLatest(endpoint_idx, buffer, &sz, 0) == Status::Success)
            {
                m_current_controller_idx = (endpoint_idx + 1) % endpoint_count; // rotate so every endpoint gets a turn
                *size = sz;
                *input_idx = endpoint_idx;
                return Status::Success;
            }
        }

        /*
         All endpoints idle: block once (up to timeout_us) on the next endpoint to pace the input
         thread - a single wait per sweep, not one per endpoint.
        */
        uint16_t endpoint_idx = m_current_controller_idx;
        m_current_controller_idx = (endpoint_idx + 1) % endpoint_count;

        size_t sz = requested_size;
        Status result = ReadEndpointLatest(endpoint_idx, buffer, &sz, timeout_us);
        if (result != Status::Success)
            return result;

        *size = sz;
        *input_idx = endpoint_idx;
        return Status::Success;
    }

    Status BaseController::ReadInput(NormalizedButtonData *normalData, uint16_t *input_idx, uint32_t timeout_us)
    {
        RawInputData rawData;
        uint8_t input_bytes[CONTROLLER_INPUT_BUFFER_SIZE];
        size_t size = std::min(sizeof(input_bytes), GetMaxInputBufferSize());

        auto read_start = std::chrono::high_resolution_clock::now();
        Status result = ReadNextBuffer(input_bytes, &size, input_idx, timeout_us);
        if (result != Status::Success)
            return result;

        auto parse_start = std::chrono::high_resolution_clock::now();
        result = ParseData(input_bytes, size, &rawData, input_idx);
        if (result != Status::Success)
            return result;

        auto map_start = std::chrono::high_resolution_clock::now();
        MapRawInputToNormalized(rawData, normalData);

        auto end = std::chrono::high_resolution_clock::now();
        if (m_logger->IsEnabled(LogLevel::Perf))
        {
            m_logger->Log(LogLevel::Perf, "Controller[%04x-%04x] Reading: %dus, Parsing: %dus, Mapping: %dus",
                          m_device->GetVendor(),
                          m_device->GetProduct(),
                          std::chrono::duration_cast<std::chrono::microseconds>(parse_start - read_start).count(),
                          std::chrono::duration_cast<std::chrono::microseconds>(map_start - parse_start).count(),
                          std::chrono::duration_cast<std::chrono::microseconds>(end - map_start).count());
        }

        return Status::Success;
    }

    class StickButton
    {
    public:
        StickButton(GamepadButton buttonId, float *axisField, float axisSign)
            : button(buttonId), value_addr(axisField), sign(axisSign) {}
        GamepadButton button;
        float *value_addr;
        float sign;
    };

    void BaseController::MapRawInputToNormalized(RawInputData &rawData, NormalizedButtonData *normalData)
    {
        normalData->motion = rawData.motion;

        if (m_logger->IsEnabled(LogLevel::Debug))
        {
            m_logger->Log(LogLevel::Debug, "Controller[%04x-%04x] B1=%d B2=%d B3=%d B4=%d B5=%d B6=%d B7=%d B8=%d B9=%d B10=%d B11=%d B12=%d B13=%d B14=%d B15=%d B16=%d B17=%d B18=%d DPAD(UP=%d RIGHT=%d DOWN=%d LEFT=%d)",
                          m_device->GetVendor(),
                          m_device->GetProduct(),
                          rawData.buttons[1] ? 1 : 0,
                          rawData.buttons[2] ? 1 : 0,
                          rawData.buttons[3] ? 1 : 0,
                          rawData.buttons[4] ? 1 : 0,
                          rawData.buttons[5] ? 1 : 0,
                          rawData.buttons[6] ? 1 : 0,
                          rawData.buttons[7] ? 1 : 0,
                          rawData.buttons[8] ? 1 : 0,
                          rawData.buttons[9] ? 1 : 0,
                          rawData.buttons[10] ? 1 : 0,
                          rawData.buttons[11] ? 1 : 0,
                          rawData.buttons[12] ? 1 : 0,
                          rawData.buttons[13] ? 1 : 0,
                          rawData.buttons[14] ? 1 : 0,
                          rawData.buttons[15] ? 1 : 0,
                          rawData.buttons[16] ? 1 : 0,
                          rawData.buttons[17] ? 1 : 0,
                          rawData.buttons[18] ? 1 : 0,
                          rawData.buttons[DPAD_UP_BUTTON_ID] ? 1 : 0,
                          rawData.buttons[DPAD_RIGHT_BUTTON_ID] ? 1 : 0,
                          rawData.buttons[DPAD_DOWN_BUTTON_ID] ? 1 : 0,
                          rawData.buttons[DPAD_LEFT_BUTTON_ID] ? 1 : 0);

            m_logger->Log(LogLevel::Debug, "Controller[%04x-%04x] X=%d%%, Y=%d%%, Z=%d%%, Rx=%d%%, Ry=%d%%, Rz=%d%%, Slider=%d%%, Dial=%d%%, Brake=%d%%, Accelerator=%d%%",
                          m_device->GetVendor(),
                          m_device->GetProduct(),
                          (int)(rawData.analog[AnalogAxis::X] * 100.0),
                          (int)(rawData.analog[AnalogAxis::Y] * 100.0),
                          (int)(rawData.analog[AnalogAxis::Z] * 100.0),
                          (int)(rawData.analog[AnalogAxis::Rx] * 100.0),
                          (int)(rawData.analog[AnalogAxis::Ry] * 100.0),
                          (int)(rawData.analog[AnalogAxis::Rz] * 100.0),
                          (int)(rawData.analog[AnalogAxis::Slider] * 100.0),
                          (int)(rawData.analog[AnalogAxis::Dial] * 100.0),
                          (int)(rawData.analog[AnalogAxis::Brake] * 100.0),
                          (int)(rawData.analog[AnalogAxis::Accelerator] * 100.0));
        }

        // Unknown is the "not bound to anything" axis; drivers never write it, and mapping reads
        // it whenever a button has no analog binding, so it has to read as centred.
        rawData.analog[AnalogAxis::Unknown] = 0.0f;

        for (AnalogAxis axis : AllAnalogAxes)
            rawData.analog[axis] = BaseController::ApplyDeadzone(GetConfig().analogDeadzonePercent[axis], rawData.analog[axis]);

        StickButton sticks_list[] = {
            // button value_addr, sign
            StickButton(GamepadButton::LSTICK_LEFT, &normalData->sticks[0].axis_x, -1.0f),
            StickButton(GamepadButton::LSTICK_RIGHT, &normalData->sticks[0].axis_x, +1.0f),
            StickButton(GamepadButton::LSTICK_UP, &normalData->sticks[0].axis_y, +1.0f),
            StickButton(GamepadButton::LSTICK_DOWN, &normalData->sticks[0].axis_y, -1.0f),
            StickButton(GamepadButton::RSTICK_LEFT, &normalData->sticks[1].axis_x, -1.0f),
            StickButton(GamepadButton::RSTICK_RIGHT, &normalData->sticks[1].axis_x, +1.0f),
            StickButton(GamepadButton::RSTICK_UP, &normalData->sticks[1].axis_y, +1.0f),
            StickButton(GamepadButton::RSTICK_DOWN, &normalData->sticks[1].axis_y, -1.0f),
        };

        // Analog value
        for (auto &&stick : sticks_list)
        {
            ControllerAnalogConfig analogCfg = GetConfig().buttonsAnalog[stick.button];
            float value = (analogCfg.sign * rawData.analog[analogCfg.bind]) * (GetConfig().analogFactorPercent[analogCfg.bind] / 100.0f);
            if (value > 1.0f)
                value = 1.0f;

            if (rawData.buttons[GetConfig().buttonsPin[stick.button][0]] || rawData.buttons[GetConfig().buttonsPin[stick.button][1]])
                *stick.value_addr = stick.sign * 1.0f;
            else if (value > 0.0f) // Is positive
                *stick.value_addr = stick.sign * value;
        }

        for (GamepadButton controllerButton : AllDigitalButtons)
            normalData->buttons[controllerButton] = rawData.buttons[GetConfig().buttonsPin[controllerButton][0]] || rawData.buttons[GetConfig().buttonsPin[controllerButton][1]];

        if (GetConfig().buttonsAnalogUsed)
        {
            for (GamepadButton controllerButton : AllDigitalButtons)
                normalData->buttons[controllerButton] |= (GetConfig().buttonsAnalog[controllerButton].sign * rawData.analog[GetConfig().buttonsAnalog[controllerButton].bind]) > 0.0f;
        }

        // Simulate buttons
        for (int i = 0; i < MAX_CONTROLLER_COMBO; i++)
        {
            const ControllerComboConfig *combo = &GetConfig().simulateCombos[i];
            if (combo->buttonSimulated == GamepadButton::NONE)
                break; // Stop at the first empty combo

            if (normalData->buttons[combo->buttons[0]] && normalData->buttons[combo->buttons[1]])
            {
                normalData->buttons[combo->buttonSimulated] = true;
                normalData->buttons[combo->buttons[0]] = false;
                normalData->buttons[combo->buttons[1]] = false;
            }
        }
    }

    /* A game is free to ask for more than full scale; hid clamps it and so must we, or the
       cast wraps and a maximum-strength effect comes out as a faint one. */
    uint32_t BaseController::ScaleAmplitude(float amplitude, uint32_t max)
    {
        if (amplitude <= 0.0f)
            return 0;

        if (amplitude >= 1.0f)
            return max;

        return (uint32_t)(amplitude * max);
    }

    float BaseController::ApplyDeadzone(uint8_t deadzonePercent, float value)
    {
        float deadzone = deadzonePercent / 100.0f;

        if (std::abs(value) < deadzone)
            return 0.0f;

        const float scale = 1.0f / (1.0f - deadzone);
        return (value > 0) ? (value - deadzone) * scale : (value + deadzone) * scale;
    }

    float BaseController::Normalize(int32_t value, int32_t min, int32_t max)
    {
        return Normalize(value, min, max, (max + min) / 2);
    }

    float BaseController::Normalize(int32_t value, int32_t min, int32_t max, int32_t center)
    {
        float ret = 0.0f;
        if (value < center)
        {
            float offset = (float)min;
            float range = (float)(center - min);
            ret = ((value - offset) / range) - 1.0f;
        }
        else
        {
            float offset = (float)center;
            float range = (float)(max - center);
            ret = (value - offset) / range;
        }

        if (ret > 1.0f)
            ret = 1.0f;
        else if (ret < -1.0f)
            ret = -1.0f;

        return ret;
    }

    uint32_t BaseController::ReadBitsLE(uint8_t *buffer, uint32_t bitOffset, uint32_t bitLength)
    {
        // Calculate the starting byte index and bit index within that byte
        uint32_t byteIndex = bitOffset / 8;
        uint32_t bitIndex = bitOffset % 8; // Little endian, LSB is at index 0

        uint32_t result = 0;

        for (uint32_t i = 0; i < bitLength; ++i)
        {
            if (bitIndex > 7)
            {
                ++byteIndex;
                bitIndex = 0;
            }

            uint8_t bit = (buffer[byteIndex] >> bitIndex) & 0x01;
            result |= (bit << i);

            ++bitIndex;
        }

        return result;
    }
} // namespace controllerlib
