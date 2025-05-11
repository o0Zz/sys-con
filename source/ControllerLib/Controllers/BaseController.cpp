#include "Controllers/BaseController.h"
#include <cmath>
#include <chrono>

// https://www.usb.org/sites/default/files/documents/hid1_11.pdf  p55

BaseController::BaseController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
    : IController(std::move(device), config, std::move(logger))
{
    Log(LogLevelDebug, "Controller[%04x-%04x] Created !", m_device->GetVendor(), m_device->GetProduct());
}

BaseController::~BaseController()
{
}

ControllerResult BaseController::Initialize()
{
    Log(LogLevelDebug, "Controller[%04x-%04x] Initializing ...", m_device->GetVendor(), m_device->GetProduct());

    ControllerResult result = OpenInterfaces();
    if (result != CONTROLLER_STATUS_SUCCESS)
    {
        Log(LogLevelError, "Controller[%04x-%04x] Failed to open interfaces !", m_device->GetVendor(), m_device->GetProduct());
        return result;
    }

    return CONTROLLER_STATUS_SUCCESS;
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

ControllerResult BaseController::OpenInterfaces()
{
    Log(LogLevelDebug, "Controller[%04x-%04x] Opening interfaces ...", m_device->GetVendor(), m_device->GetProduct());

    ControllerResult result = m_device->Open();
    if (result != CONTROLLER_STATUS_SUCCESS)
    {
        Log(LogLevelError, "Controller[%04x-%04x] Failed to open device !", m_device->GetVendor(), m_device->GetProduct());
        return result;
    }

    std::vector<std::unique_ptr<IUSBInterface>> &interfaces = m_device->GetInterfaces();
    for (auto &&interface : interfaces)
    {
        Log(LogLevelDebug, "Controller[%04x-%04x] Opening interface %d/%d ...", m_device->GetVendor(), m_device->GetProduct(), m_interfaces.size() + 1, interfaces.size());

        ControllerResult result = interface->Open();
        if (result != CONTROLLER_STATUS_SUCCESS)
        {
            Log(LogLevelError, "Controller[%04x-%04x] Failed to open interface !", m_device->GetVendor(), m_device->GetProduct());
            return result;
        }

        for (uint8_t idx = 0; idx < 15; idx++)
        {
            IUSBEndpoint *inEndpoint = interface->GetEndpoint(IUSBEndpoint::USB_ENDPOINT_IN, idx);
            if (inEndpoint == NULL)
                continue;

            ControllerResult result = inEndpoint->Open(GetConfig().inputMaxPacketSize);
            if (result != CONTROLLER_STATUS_SUCCESS)
            {
                Log(LogLevelError, "Controller[%04x-%04x] Failed to open input endpoint idx: %d !", m_device->GetVendor(), m_device->GetProduct(), idx);
                return result;
            }

            m_inPipe.push_back(inEndpoint);
        }

        for (uint8_t idx = 0; idx < 15; idx++)
        {
            IUSBEndpoint *outEndpoint = interface->GetEndpoint(IUSBEndpoint::USB_ENDPOINT_OUT, idx);
            if (outEndpoint == NULL)
                continue;

            result = outEndpoint->Open(GetConfig().outputMaxPacketSize);
            if (result != CONTROLLER_STATUS_SUCCESS)
            {
                Log(LogLevelError, "Controller[%04x-%04x] Failed to open output endpoint idx: %d !", m_device->GetVendor(), m_device->GetProduct(), idx);
                return result;
            }

            m_outPipe.push_back(outEndpoint);
        }

        m_interfaces.push_back(interface.get());
    }

    if (m_inPipe.empty())
    {
        Log(LogLevelError, "Controller[%04x-%04x] Not input endpoint found !", m_device->GetVendor(), m_device->GetProduct());
        return CONTROLLER_STATUS_INVALID_ENDPOINT;
    }

    Log(LogLevelDebug, "Controller[%04x-%04x] successfully opened !", m_device->GetVendor(), m_device->GetProduct());
    return CONTROLLER_STATUS_SUCCESS;
}

void BaseController::CloseInterfaces()
{
    m_device->Close();
}

bool BaseController::Support(ControllerFeature feature)
{
    (void)feature;
    return false;
}

ControllerResult BaseController::SetRumble(uint16_t input_idx, float amp_high, float amp_low)
{
    (void)input_idx;
    (void)amp_high;
    (void)amp_low;
    // Not implemented yet
    return CONTROLLER_STATUS_NOT_IMPLEMENTED;
}

ControllerResult BaseController::ReadNextBuffer(uint8_t *buffer, size_t *size, uint16_t *input_idx, uint32_t timeout_us)
{
    uint16_t controller_idx = m_current_controller_idx;
    m_current_controller_idx = (m_current_controller_idx + 1) % m_inPipe.size();

    *size = std::min((size_t)m_inPipe[controller_idx]->GetDescriptor()->wMaxPacketSize, *size);

    ControllerResult result = m_inPipe[controller_idx]->Read(buffer, size, timeout_us);
    if (result != CONTROLLER_STATUS_SUCCESS)
        return result;

    if (*size == 0)
        return CONTROLLER_STATUS_NOTHING_TODO;

    *input_idx = controller_idx;

    return CONTROLLER_STATUS_SUCCESS;
}

ControllerResult BaseController::ReadInput(NormalizedButtonData *normalData, uint16_t *input_idx, uint32_t timeout_us)
{
    RawInputData rawData;
    uint8_t input_bytes[CONTROLLER_INPUT_BUFFER_SIZE];
    size_t size = std::min(sizeof(input_bytes), GetMaxInputBufferSize());

    auto read_start = std::chrono::high_resolution_clock::now();
    ControllerResult result = ReadNextBuffer(input_bytes, &size, input_idx, timeout_us);
    if (result != CONTROLLER_STATUS_SUCCESS)
        return result;

    auto parse_start = std::chrono::high_resolution_clock::now();
    result = ParseData(input_bytes, size, &rawData, input_idx);
    if (result != CONTROLLER_STATUS_SUCCESS)
        return result;

    auto map_start = std::chrono::high_resolution_clock::now();
    MapRawInputToNormalized(rawData, normalData);

    auto end = std::chrono::high_resolution_clock::now();
    Log(LogLevelPerf, "Controller[%04x-%04x] Reading: %dms, Parsing: %dms, Mapping: %dms",
        m_device->GetVendor(),
        m_device->GetProduct(),
        std::chrono::duration_cast<std::chrono::milliseconds>(parse_start - read_start).count(),
        std::chrono::duration_cast<std::chrono::milliseconds>(map_start - parse_start).count(),
        std::chrono::duration_cast<std::chrono::milliseconds>(end - map_start).count());

    return CONTROLLER_STATUS_SUCCESS;
}

class StickButton
{
public:
    StickButton(ControllerButton button, float *value_addr, float sign)
        : button(button), value_addr(value_addr), sign(sign) {}
    ControllerButton button;
    float *value_addr;
    float sign;
};

void BaseController::MapRawInputToNormalized(RawInputData &rawData, NormalizedButtonData *normalData)
{
    Log(LogLevelDebug, "Controller[%04x-%04x] DATA: X=%d%%, Y=%d%%, Z=%d%%, Rz=%d%%, B1=%d, B2=%d, B3=%d, B4=%d, B5=%d, B6=%d, B7=%d, B8=%d, B9=%d, B10=%d",
        m_device->GetVendor(), m_device->GetProduct(),
        (int)(rawData.analog[ControllerAnalogBinding_X] * 100.0), (int)(rawData.analog[ControllerAnalogBinding_Y] * 100.0), (int)(rawData.analog[ControllerAnalogBinding_Z] * 100.0), (int)(rawData.analog[ControllerAnalogBinding_Rz] * 100.0),
        rawData.buttons[1] ? 1 : 0,
        rawData.buttons[2] ? 1 : 0,
        rawData.buttons[3] ? 1 : 0,
        rawData.buttons[4] ? 1 : 0,
        rawData.buttons[5] ? 1 : 0,
        rawData.buttons[6] ? 1 : 0,
        rawData.buttons[7] ? 1 : 0,
        rawData.buttons[8] ? 1 : 0,
        rawData.buttons[9] ? 1 : 0,
        rawData.buttons[10] ? 1 : 0);

    rawData.analog[ControllerAnalogBinding_Unknown] = 0.0f;
    rawData.analog[ControllerAnalogBinding_X] = BaseController::ApplyDeadzone(GetConfig().analogDeadzonePercent[ControllerAnalogBinding_X], rawData.analog[ControllerAnalogBinding_X]);
    rawData.analog[ControllerAnalogBinding_Y] = BaseController::ApplyDeadzone(GetConfig().analogDeadzonePercent[ControllerAnalogBinding_Y], rawData.analog[ControllerAnalogBinding_Y]);
    rawData.analog[ControllerAnalogBinding_Z] = BaseController::ApplyDeadzone(GetConfig().analogDeadzonePercent[ControllerAnalogBinding_Z], rawData.analog[ControllerAnalogBinding_Z]);
    rawData.analog[ControllerAnalogBinding_Rz] = BaseController::ApplyDeadzone(GetConfig().analogDeadzonePercent[ControllerAnalogBinding_Rz], rawData.analog[ControllerAnalogBinding_Rz]);
    rawData.analog[ControllerAnalogBinding_Rx] = BaseController::ApplyDeadzone(GetConfig().analogDeadzonePercent[ControllerAnalogBinding_Rx], rawData.analog[ControllerAnalogBinding_Rx]);
    rawData.analog[ControllerAnalogBinding_Ry] = BaseController::ApplyDeadzone(GetConfig().analogDeadzonePercent[ControllerAnalogBinding_Ry], rawData.analog[ControllerAnalogBinding_Ry]);
    rawData.analog[ControllerAnalogBinding_Slider] = BaseController::ApplyDeadzone(GetConfig().analogDeadzonePercent[ControllerAnalogBinding_Slider], rawData.analog[ControllerAnalogBinding_Slider]);
    rawData.analog[ControllerAnalogBinding_Dial] = BaseController::ApplyDeadzone(GetConfig().analogDeadzonePercent[ControllerAnalogBinding_Dial], rawData.analog[ControllerAnalogBinding_Dial]);

    StickButton sticks_list[8] = {
        // button value_addr, sign
        StickButton(ControllerButton::LSTICK_LEFT, &normalData->sticks[0].axis_x, -1.0f),
        StickButton(ControllerButton::LSTICK_RIGHT, &normalData->sticks[0].axis_x, +1.0f),
        StickButton(ControllerButton::LSTICK_UP, &normalData->sticks[0].axis_y, +1.0f),
        StickButton(ControllerButton::LSTICK_DOWN, &normalData->sticks[0].axis_y, -1.0f),
        StickButton(ControllerButton::RSTICK_LEFT, &normalData->sticks[1].axis_x, -1.0f),
        StickButton(ControllerButton::RSTICK_RIGHT, &normalData->sticks[1].axis_x, +1.0f),
        StickButton(ControllerButton::RSTICK_UP, &normalData->sticks[1].axis_y, +1.0f),
        StickButton(ControllerButton::RSTICK_DOWN, &normalData->sticks[1].axis_y, -1.0f),
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

    static_assert(MAX_PIN_BY_BUTTONS == 2, "You need to update IsPinPressed macro !");

#define IsButtonPressed(rawData, controllerButton)                                                                               \
    (rawData.buttons[GetConfig().buttonsPin[controllerButton][0]] ||                                                             \
     rawData.buttons[GetConfig().buttonsPin[controllerButton][1]] ||                                                             \
     GetConfig().buttonsAnalog[controllerButton].sign * rawData.analog[GetConfig().buttonsAnalog[controllerButton].bind] > 0.0f) \
        ? true                                                                                                                   \
        : false

    // Set Buttons
    normalData->buttons[ControllerButton::X] = IsButtonPressed(rawData, ControllerButton::X);
    normalData->buttons[ControllerButton::A] = IsButtonPressed(rawData, ControllerButton::A);
    normalData->buttons[ControllerButton::B] = IsButtonPressed(rawData, ControllerButton::B);
    normalData->buttons[ControllerButton::Y] = IsButtonPressed(rawData, ControllerButton::Y);
    normalData->buttons[ControllerButton::LSTICK_CLICK] = IsButtonPressed(rawData, ControllerButton::LSTICK_CLICK);
    normalData->buttons[ControllerButton::RSTICK_CLICK] = IsButtonPressed(rawData, ControllerButton::RSTICK_CLICK);
    normalData->buttons[ControllerButton::L] = IsButtonPressed(rawData, ControllerButton::L);
    normalData->buttons[ControllerButton::R] = IsButtonPressed(rawData, ControllerButton::R);
    normalData->buttons[ControllerButton::ZL] = IsButtonPressed(rawData, ControllerButton::ZL);
    normalData->buttons[ControllerButton::ZR] = IsButtonPressed(rawData, ControllerButton::ZR);
    normalData->buttons[ControllerButton::MINUS] = IsButtonPressed(rawData, ControllerButton::MINUS);
    normalData->buttons[ControllerButton::PLUS] = IsButtonPressed(rawData, ControllerButton::PLUS);
    normalData->buttons[ControllerButton::CAPTURE] = IsButtonPressed(rawData, ControllerButton::CAPTURE);
    normalData->buttons[ControllerButton::HOME] = IsButtonPressed(rawData, ControllerButton::HOME);
    normalData->buttons[ControllerButton::DPAD_UP] = IsButtonPressed(rawData, ControllerButton::DPAD_UP);
    normalData->buttons[ControllerButton::DPAD_DOWN] = IsButtonPressed(rawData, ControllerButton::DPAD_DOWN);
    normalData->buttons[ControllerButton::DPAD_RIGHT] = IsButtonPressed(rawData, ControllerButton::DPAD_RIGHT);
    normalData->buttons[ControllerButton::DPAD_LEFT] = IsButtonPressed(rawData, ControllerButton::DPAD_LEFT);

    // Simulate combos for any button
    for (int btn = 0; btn < ControllerButton::COUNT; ++btn)
    {
        const ControllerButton *combo = GetConfig().simulateCombos[btn];
        if (combo[0] != ControllerButton::NONE && combo[1] != ControllerButton::NONE)
        {
            if (normalData->buttons[combo[0]] && normalData->buttons[combo[1]])
            {
                normalData->buttons[btn] = true;
                normalData->buttons[combo[0]] = false;
                normalData->buttons[combo[1]] = false;
            }
        }
    }
}

float BaseController::ApplyDeadzone(uint8_t deadzonePercent, float value)
{
    float deadzone = 1.0f * deadzonePercent / 100.0f;

    if (std::abs(value) < deadzone)
        return 0.0f;

    if (value > 0)
        value = (value - deadzone) / (1.0f - deadzone);
    else
        value = (value + deadzone) / (1.0f - deadzone);

    return value;
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
        // Check if we need to move to the next byte
        if (bitIndex > 7)
        {
            ++byteIndex;
            bitIndex = 0;
        }

        // Get the bit at the current position and add it to the result
        uint8_t bit = (buffer[byteIndex] >> bitIndex) & 0x01;
        result |= (bit << i);

        // Move to the next bit
        ++bitIndex;
    }

    return result;
}