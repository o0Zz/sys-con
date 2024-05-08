#include "Controllers/Dualshock3Controller.h"

Dualshock3Controller::Dualshock3Controller(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
    : BaseController(std::move(device), config, std::move(logger))
{
}

Dualshock3Controller::~Dualshock3Controller()
{
}

ams::Result Dualshock3Controller::Initialize()
{
    R_TRY(BaseController::Initialize());

    SetLED(DS3LED_1);

    R_SUCCEED();
}

ams::Result Dualshock3Controller::OpenInterfaces()
{
    R_TRY(m_device->Open());

    std::vector<std::unique_ptr<IUSBInterface>> &interfaces = m_device->GetInterfaces();
    for (auto &&interface : interfaces)
    {
        R_TRY(interface->Open());

        // Send an initial control packet
        constexpr uint8_t initBytes[] = {0x42, 0x0C, 0x00, 0x00};
        R_TRY(SendCommand(interface.get(), Ds3FeatureStartDevice, initBytes, sizeof(initBytes)));

        m_interface = interface.get();

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

ams::Result Dualshock3Controller::ReadInput(NormalizedButtonData *normalData, uint16_t *input_idx)
{
    uint8_t input_bytes[49];
    size_t size = sizeof(input_bytes);

    R_TRY(m_inPipe->Read(input_bytes, &size));

    if (input_bytes[0] == Ds3InputPacket_Button)
    {
        Dualshock3ButtonData *buttonData = reinterpret_cast<Dualshock3ButtonData *>(input_bytes);

        *input_idx = 0;

        normalData->triggers[0] = NormalizeTrigger(GetConfig().triggerDeadzonePercent[0], buttonData->trigger_left_pressure);
        normalData->triggers[1] = NormalizeTrigger(GetConfig().triggerDeadzonePercent[1], buttonData->trigger_right_pressure);

        NormalizeAxis(buttonData->stick_left_x, buttonData->stick_left_y, GetConfig().stickDeadzonePercent[0],
                      &normalData->sticks[0].axis_x, &normalData->sticks[0].axis_y, 0, 255);
        NormalizeAxis(buttonData->stick_right_x, buttonData->stick_right_y, GetConfig().stickDeadzonePercent[1],
                      &normalData->sticks[1].axis_x, &normalData->sticks[1].axis_y, 0, 255);

        bool buttons[MAX_CONTROLLER_BUTTONS] = {
            buttonData->triangle, // X
            buttonData->circle,   // A
            buttonData->cross,    // B
            buttonData->square,   // Y
            buttonData->stick_left_click,
            buttonData->stick_right_click,
            buttonData->bumper_left,     // L
            buttonData->bumper_right,    // R
            normalData->triggers[0] > 0, // ZL
            normalData->triggers[1] > 0, // ZR
            buttonData->back,            // Minus
            buttonData->start,           // Plus
            buttonData->dpad_up,         // UP
            buttonData->dpad_right,      // RIGHT
            buttonData->dpad_down,       // DOWN
            buttonData->dpad_left,       // LEFT
            false,                       // Capture
            buttonData->guide,           // Home
        };

        for (int i = 0; i != MAX_CONTROLLER_BUTTONS; ++i)
        {
            ControllerButton button = GetConfig().buttons[i];
            if (button == NONE)
                continue;

            normalData->buttons[(button != DEFAULT ? button - 2 : i)] += buttons[i];
        }
    }

    R_SUCCEED();
}

ams::Result Dualshock3Controller::SendCommand(IUSBInterface *interface, Dualshock3FeatureValue feature, const void *buffer, uint16_t size)
{
    R_RETURN(interface->ControlTransferOutput(USB_ENDPOINT_OUT | 0x21, 0x09, static_cast<uint16_t>(feature), 0, buffer, size));
}

ams::Result Dualshock3Controller::SetLED(Dualshock3LEDValue value)
{
    const uint8_t ledPacket[]{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>(value << 1),
        LED_PERMANENT,
        LED_PERMANENT,
        LED_PERMANENT,
        LED_PERMANENT};
    R_RETURN(SendCommand(m_interface, Ds3FeatureUnknown1, ledPacket, sizeof(ledPacket)));
}
