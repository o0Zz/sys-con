#include "drivers/Xbox360Controller.h"

namespace controllerlib
{
    // https://www.partsnotincluded.com/understanding-the-xbox-360-wired-controllers-usb-data/

    Xbox360Controller::Xbox360Controller(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger))
    {
    }

    Xbox360Controller::~Xbox360Controller()
    {
    }

    Status Xbox360Controller::Initialize()
    {
        Status result = BaseController::Initialize();
        if (result != Status::Success)
            return result;

        SetLED(0, XBOX360LED_TOPLEFT);

        return Status::Success;
    }

    Status Xbox360Controller::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        (void)input_idx;
        Xbox360ButtonData *buttonData = reinterpret_cast<Xbox360ButtonData *>(buffer);

        if (size < sizeof(Xbox360ButtonData))
            return Status::UnexpectedData;

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

            rawData->analog[AnalogAxis::Rx] = BaseController::Normalize(buttonData->Rx, 0, 255);
            rawData->analog[AnalogAxis::Ry] = BaseController::Normalize(buttonData->Ry, 0, 255);

            rawData->analog[AnalogAxis::X] = BaseController::Normalize(buttonData->X, -32768, 32767);
            rawData->analog[AnalogAxis::Y] = BaseController::Normalize(-buttonData->Y, -32768, 32767);
            rawData->analog[AnalogAxis::Z] = BaseController::Normalize(buttonData->Z, -32768, 32767);
            rawData->analog[AnalogAxis::Rz] = BaseController::Normalize(-buttonData->Rz, -32768, 32767);

            rawData->buttons[DPAD_UP_BUTTON_ID] = buttonData->dpad_up;
            rawData->buttons[DPAD_RIGHT_BUTTON_ID] = buttonData->dpad_right;
            rawData->buttons[DPAD_DOWN_BUTTON_ID] = buttonData->dpad_down;
            rawData->buttons[DPAD_LEFT_BUTTON_ID] = buttonData->dpad_left;

            return Status::Success;
        }

        return Status::UnexpectedData;
    }

    bool Xbox360Controller::Support(ControllerFeature feature)
    {
        if (feature == SUPPORTS_RUMBLE)
            return true;

        return false;
    }

    Status Xbox360Controller::SetRumble(uint16_t input_idx, float amp_high, float amp_low)
    {
        uint8_t rumbleData[]{0x00, 0x08, 0x00, (uint8_t)(amp_high * 255), (uint8_t)(amp_low * 255), 0x00, 0x00, 0x00};
        if (m_outPipe.size() <= input_idx)
            return Status::InvalidIndex;

        return m_outPipe[input_idx]->Write(rumbleData, sizeof(rumbleData));
    }

    Status Xbox360Controller::SetLED(uint16_t input_idx, Xbox360LEDValue value)
    {
        uint8_t ledPacket[]{0x01, 0x03, (uint8_t)(value)};
        if (m_outPipe.size() <= input_idx)
            return Status::Success;

        return m_outPipe[input_idx]->Write(ledPacket, sizeof(ledPacket));
    }
} // namespace controllerlib
