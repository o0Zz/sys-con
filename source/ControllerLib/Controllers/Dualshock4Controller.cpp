#include "Controllers/Dualshock4Controller.h"
#include <cmath>

Dualshock4Controller::Dualshock4Controller(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
    : IController(std::move(device), config, std::move(logger))
{
}

Dualshock4Controller::~Dualshock4Controller()
{
}

ams::Result Dualshock4Controller::SendInitBytes()
{
    const uint8_t init_bytes[32] = {
        0x05, 0x07, 0x00, 0x00,
        0x00, 0x00,                                                             // initial strong and weak rumble
        GetConfig().ledColor.r, GetConfig().ledColor.g, GetConfig().ledColor.b, // LED color
        0x00, 0x00};

    R_RETURN(m_outPipe->Write(init_bytes, sizeof(init_bytes)));
}

ams::Result Dualshock4Controller::Initialize()
{
    R_TRY(OpenInterfaces());
    R_TRY(SendInitBytes());

    R_SUCCEED();
}

void Dualshock4Controller::Exit()
{
    CloseInterfaces();
}

ams::Result Dualshock4Controller::OpenInterfaces()
{
    R_TRY(m_device->Open());

    // Open each interface, send it a setup packet and get the endpoints if it succeeds
    std::vector<std::unique_ptr<IUSBInterface>> &interfaces = m_device->GetInterfaces();
    for (auto &&interface : interfaces)
    {
        R_TRY(interface->Open());

        if (interface->GetDescriptor()->bInterfaceClass != 3)
            continue;

        if (interface->GetDescriptor()->bInterfaceProtocol != 0)
            continue;

        if (interface->GetDescriptor()->bNumEndpoints < 2)
            continue;

        if (!m_inPipe)
        {
            for (int i = 0; i != 15; ++i)
            {
                IUSBEndpoint *inEndpoint = interface->GetEndpoint(IUSBEndpoint::USB_ENDPOINT_IN, i);
                if (inEndpoint)
                {
                    R_TRY(inEndpoint->Open());

                    m_inPipe = inEndpoint;
                    break;
                }
            }
        }

        if (!m_outPipe)
        {
            for (int i = 0; i != 15; ++i)
            {
                IUSBEndpoint *outEndpoint = interface->GetEndpoint(IUSBEndpoint::USB_ENDPOINT_OUT, i);
                if (outEndpoint)
                {
                    R_TRY(outEndpoint->Open());

                    m_outPipe = outEndpoint;
                    break;
                }
            }
        }
    }

    if (!m_inPipe || !m_outPipe)
        R_RETURN(CONTROL_ERR_INVALID_ENDPOINT);

    R_SUCCEED();
}

void Dualshock4Controller::CloseInterfaces()
{
    // m_device->Reset();
    m_device->Close();
}

ams::Result Dualshock4Controller::GetInput()
{
    uint8_t input_bytes[64];
    size_t size = sizeof(input_bytes);

    R_TRY(m_inPipe->Read(input_bytes, &size));

    if (input_bytes[0] == 0x01)
    {
        m_buttonData = *reinterpret_cast<Dualshock4USBButtonData *>(input_bytes);
    }

    R_SUCCEED();
}

bool Dualshock4Controller::Support(ControllerFeature feature)
{
    switch (feature)
    {
        case SUPPORTS_RUMBLE:
            return true;
        case SUPPORTS_BLUETOOTH:
            return true;
        case SUPPORTS_SIXAXIS:
            return true;
        default:
            return false;
    }
};

// Pass by value should hopefully be optimized away by RVO
NormalizedButtonData Dualshock4Controller::GetNormalizedButtonData()
{
    NormalizedButtonData normalData{};

    normalData.triggers[0] = NormalizeTrigger(GetConfig().triggerDeadzonePercent[0], m_buttonData.l2_pressure);
    normalData.triggers[1] = NormalizeTrigger(GetConfig().triggerDeadzonePercent[1], m_buttonData.r2_pressure);

    NormalizeAxis(m_buttonData.stick_left_x, m_buttonData.stick_left_y, GetConfig().stickDeadzonePercent[0],
                  &normalData.sticks[0].axis_x, &normalData.sticks[0].axis_y, 0, 255);
    NormalizeAxis(m_buttonData.stick_right_x, m_buttonData.stick_right_y, GetConfig().stickDeadzonePercent[1],
                  &normalData.sticks[1].axis_x, &normalData.sticks[1].axis_y, 0, 255);

    bool buttons[MAX_CONTROLLER_BUTTONS] = {
        m_buttonData.triangle,
        m_buttonData.circle,
        m_buttonData.cross,
        m_buttonData.square,
        m_buttonData.l3,
        m_buttonData.r3,
        m_buttonData.l1,
        m_buttonData.r1,
        m_buttonData.l2,
        m_buttonData.r2,
        m_buttonData.share,
        m_buttonData.options,
        (m_buttonData.dpad == DS4_UP) || (m_buttonData.dpad == DS4_UPRIGHT) || (m_buttonData.dpad == DS4_UPLEFT),
        (m_buttonData.dpad == DS4_RIGHT) || (m_buttonData.dpad == DS4_UPRIGHT) || (m_buttonData.dpad == DS4_DOWNRIGHT),
        (m_buttonData.dpad == DS4_DOWN) || (m_buttonData.dpad == DS4_DOWNRIGHT) || (m_buttonData.dpad == DS4_DOWNLEFT),
        (m_buttonData.dpad == DS4_LEFT) || (m_buttonData.dpad == DS4_UPLEFT) || (m_buttonData.dpad == DS4_DOWNLEFT),
        m_buttonData.touchpad_press,
        m_buttonData.psbutton,
        false,
        m_buttonData.touchpad_finger1_unpressed == false,
    };

    for (int i = 0; i != MAX_CONTROLLER_BUTTONS; ++i)
    {
        ControllerButton button = GetConfig().buttons[i];
        if (button == NONE)
            continue;

        normalData.buttons[(button != DEFAULT ? button - 2 : i)] += buttons[i];
    }

    return normalData;
}

ams::Result Dualshock4Controller::SetRumble(uint8_t strong_magnitude, uint8_t weak_magnitude)
{
    AMS_UNUSED(strong_magnitude);
    AMS_UNUSED(weak_magnitude);

    // Not implemented yet
    R_RETURN(CONTROL_ERR_NOT_IMPLEMENTED);
}
