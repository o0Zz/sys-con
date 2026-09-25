#include "drivers/XboxController.h"

namespace controllerlib
{
    XboxController::XboxController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger))
    {
    }

    XboxController::~XboxController()
    {
    }

    Status XboxController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        (void)input_idx;

        XboxButtonData *buttonData = reinterpret_cast<XboxButtonData *>(buffer);

        if (size < sizeof(XboxButtonData))
            return Status::UnexpectedData;

        rawData->buttons[1] = buttonData->button1 > 0;
        rawData->buttons[2] = buttonData->button2 > 0;
        rawData->buttons[3] = buttonData->button3 > 0;
        rawData->buttons[4] = buttonData->button4 > 0;
        rawData->buttons[5] = buttonData->button5 > 0;
        rawData->buttons[6] = buttonData->button6 > 0;
        rawData->buttons[7] = buttonData->button7;
        rawData->buttons[8] = buttonData->button8;
        rawData->buttons[9] = buttonData->button9;
        rawData->buttons[10] = buttonData->button10;

        rawData->analog[AnalogAxis::Rx] = BaseController::Normalize(buttonData->trigger_left, 0, 255);
        rawData->analog[AnalogAxis::Ry] = BaseController::Normalize(buttonData->trigger_right, 0, 255);
        rawData->analog[AnalogAxis::X] = BaseController::Normalize(buttonData->stick_left_x, -32768, 32767);
        rawData->analog[AnalogAxis::Y] = BaseController::Normalize(-buttonData->stick_left_y, -32768, 32767);
        rawData->analog[AnalogAxis::Z] = BaseController::Normalize(buttonData->stick_right_x, -32768, 32767);
        rawData->analog[AnalogAxis::Rz] = BaseController::Normalize(-buttonData->stick_right_y, -32768, 32767);

        rawData->buttons[DPAD_UP_BUTTON_ID] = buttonData->dpad_up;
        rawData->buttons[DPAD_RIGHT_BUTTON_ID] = buttonData->dpad_right;
        rawData->buttons[DPAD_DOWN_BUTTON_ID] = buttonData->dpad_down;
        rawData->buttons[DPAD_LEFT_BUTTON_ID] = buttonData->dpad_left;

        return Status::Success;
    }

    Status XboxController::SetRumble(uint16_t input_idx, const RumbleValue &rumble)
    {
        if (m_outPipe.size() <= input_idx)
            return Status::InvalidIndex;

        // Both motors are 16 bit little endian, so the amplitude goes in the high byte.
        const uint8_t rumbleData[]{0x00, 0x06,
                                   0x00, (uint8_t)ScaleAmplitude(rumble.LowAmplitude(), 255),
                                   0x00, (uint8_t)ScaleAmplitude(rumble.HighAmplitude(), 255)};

        return m_outPipe[input_idx]->Write(rumbleData, sizeof(rumbleData));
    }
} // namespace controllerlib
