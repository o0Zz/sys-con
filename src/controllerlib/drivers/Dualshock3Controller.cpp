#include "drivers/Dualshock3Controller.h"

#define LED_PERMANENT 0xff, 0x27, 0x00, 0x00, 0x32

namespace controllerlib
{
    static_assert(sizeof(Dualshock3ButtonData) == 49);

    Dualshock3Controller::Dualshock3Controller(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger))
    {
    }

    Dualshock3Controller::~Dualshock3Controller()
    {
    }

    Status Dualshock3Controller::Initialize()
    {
        Status result = BaseController::Initialize();
        if (result != Status::Success)
            return result;

        return SendOutputReport(DS3LED_1);
    }

    Status Dualshock3Controller::OpenInterfaces()
    {
        Status result = BaseController::OpenInterfaces();
        if (result != Status::Success)
            return result;

        constexpr uint8_t initBytes[] = {0x42, 0x0C, 0x00, 0x00};
        return SendCommand(Ds3FeatureStartDevice, initBytes, sizeof(initBytes));
    }

    Status Dualshock3Controller::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        (void)input_idx;
        Dualshock3ButtonData *buttonData = reinterpret_cast<Dualshock3ButtonData *>(buffer);

        if (size < sizeof(Dualshock3ButtonData))
            return Status::UnexpectedData;

        if (buttonData->type == Ds3InputPacket_Button)
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
            rawData->buttons[12] = buttonData->button12;
            rawData->buttons[13] = buttonData->button13;

            rawData->analog[AnalogAxis::Rx] = BaseController::Normalize(buttonData->Rx, 0, 255);
            rawData->analog[AnalogAxis::Ry] = BaseController::Normalize(buttonData->Ry, 0, 255);
            rawData->analog[AnalogAxis::X] = BaseController::Normalize(buttonData->X, 0, 255);
            rawData->analog[AnalogAxis::Y] = BaseController::Normalize(buttonData->Y, 0, 255);
            rawData->analog[AnalogAxis::Z] = BaseController::Normalize(buttonData->Z, 0, 255);
            rawData->analog[AnalogAxis::Rz] = BaseController::Normalize(buttonData->Rz, 0, 255);

            rawData->buttons[DPAD_UP_BUTTON_ID] = buttonData->dpad_up;
            rawData->buttons[DPAD_RIGHT_BUTTON_ID] = buttonData->dpad_right;
            rawData->buttons[DPAD_DOWN_BUTTON_ID] = buttonData->dpad_down;
            rawData->buttons[DPAD_LEFT_BUTTON_ID] = buttonData->dpad_left;

            return Status::Success;
        }

        return Status::UnexpectedData;
    }

    Status Dualshock3Controller::SendCommand(Dualshock3FeatureValue feature, const void *buffer, uint16_t size)
    {
        return m_interfaces[0]->ControlTransferOutput(0x21, 0x09, static_cast<uint16_t>(feature), 0, buffer, size);
    }

    /*
        One report carries both the LEDs and the motors, so the rumble state has to be resent
        with every LED change and the LEDs with every rumble change. The small motor is on/off
        only; the big one takes a force. 0xff is an endless duration.
    */
    Status Dualshock3Controller::SendOutputReport(Dualshock3LEDValue led)
    {
        const uint8_t outputPacket[]{
            0x00,
            0xff, static_cast<uint8_t>(m_rumble_right_on ? 0x01 : 0x00),
            0xff, m_rumble_left_force,
            0x00, 0x00, 0x00, 0x00,
            static_cast<uint8_t>(led << 1),
            LED_PERMANENT,
            LED_PERMANENT,
            LED_PERMANENT,
            LED_PERMANENT};
        return SendCommand(Ds3FeatureUnknown1, outputPacket, sizeof(outputPacket));
    }

    Status Dualshock3Controller::SetRumble(uint16_t input_idx, const RumbleValue &rumble)
    {
        if (input_idx != 0)
            return Status::InvalidIndex;

        if (m_interfaces.empty())
            return Status::InvalidEndpoint;

        m_rumble_left_force = static_cast<uint8_t>(ScaleAmplitude(rumble.LowAmplitude(), 255));
        m_rumble_right_on = rumble.HighAmplitude() > 0.0f;

        return SendOutputReport(DS3LED_1);
    }
} // namespace controllerlib
