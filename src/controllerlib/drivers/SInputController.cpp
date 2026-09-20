#include "drivers/SInputController.h"

namespace controllerlib
{
    SInputController::SInputController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger))
    {
    }

    SInputController::~SInputController()
    {
    }

    Status SInputController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        (void)input_idx;

        if (size < 1)
            return Status::UnexpectedData;

        // 0x02 carries replies to the command report, which this driver never sends.
        if (buffer[0] != SINPUT_REPORT_ID_INPUT)
            return Status::NothingTodo;

        if (size < sizeof(SInputButtonData))
            return Status::UnexpectedData;

        SInputButtonData *buttonData = reinterpret_cast<SInputButtonData *>(buffer);

        /*
            The protocol carries 32 button bits, four of them the d-pad. The d-pad goes to the
            pseudo-pins like every other driver, so the remaining 28 are numbered 1..28 in wire
            order -- SInput button bit N would otherwise land on pin 32, which is DPAD_UP.
        */
        rawData->buttons[1] = buttonData->south;
        rawData->buttons[2] = buttonData->east;
        rawData->buttons[3] = buttonData->west;
        rawData->buttons[4] = buttonData->north;
        rawData->buttons[5] = buttonData->stick_left;
        rawData->buttons[6] = buttonData->stick_right;
        rawData->buttons[7] = buttonData->l_bumper;
        rawData->buttons[8] = buttonData->r_bumper;
        rawData->buttons[9] = buttonData->l_trigger;
        rawData->buttons[10] = buttonData->r_trigger;
        rawData->buttons[11] = buttonData->l_paddle_1;
        rawData->buttons[12] = buttonData->r_paddle_1;
        rawData->buttons[13] = buttonData->start;
        rawData->buttons[14] = buttonData->select;
        rawData->buttons[15] = buttonData->guide;
        rawData->buttons[16] = buttonData->share;
        rawData->buttons[17] = buttonData->l_paddle_2;
        rawData->buttons[18] = buttonData->r_paddle_2;
        rawData->buttons[19] = buttonData->l_touchpad;
        rawData->buttons[20] = buttonData->r_touchpad;
        rawData->buttons[21] = buttonData->power;
        rawData->buttons[22] = buttonData->misc_4;
        rawData->buttons[23] = buttonData->misc_5;
        rawData->buttons[24] = buttonData->misc_6;
        rawData->buttons[25] = buttonData->misc_7;
        rawData->buttons[26] = buttonData->misc_8;
        rawData->buttons[27] = buttonData->misc_9;
        rawData->buttons[28] = buttonData->misc_10;

        /*
            SInput exists to be read by SDL without a translation layer, so its axes are already
            in the convention this library normalizes to: +X right, +Y down, triggers running
            from INT16_MIN (released) to INT16_MAX. Nothing is inverted here, unlike the drivers
            for pads that report +Y up.
        */
        rawData->analog[AnalogAxis::X] = BaseController::Normalize(buttonData->left_x, -32768, 32767);
        rawData->analog[AnalogAxis::Y] = BaseController::Normalize(buttonData->left_y, -32768, 32767);
        rawData->analog[AnalogAxis::Z] = BaseController::Normalize(buttonData->right_x, -32768, 32767);
        rawData->analog[AnalogAxis::Rz] = BaseController::Normalize(buttonData->right_y, -32768, 32767);
        rawData->analog[AnalogAxis::Rx] = BaseController::Normalize(buttonData->trigger_l, -32768, 32767);
        rawData->analog[AnalogAxis::Ry] = BaseController::Normalize(buttonData->trigger_r, -32768, 32767);

        rawData->buttons[DPAD_UP_BUTTON_ID] = buttonData->dpad_up;
        rawData->buttons[DPAD_RIGHT_BUTTON_ID] = buttonData->dpad_right;
        rawData->buttons[DPAD_DOWN_BUTTON_ID] = buttonData->dpad_down;
        rawData->buttons[DPAD_LEFT_BUTTON_ID] = buttonData->dpad_left;

        return Status::Success;
    }

    bool SInputController::Support(ControllerFeature feature)
    {
        if (feature == SUPPORTS_RUMBLE)
            return true;

        return false;
    }

    Status SInputController::SetRumble(uint16_t input_idx, float amp_high, float amp_low)
    {
        if (m_outPipe.size() <= input_idx)
            return Status::InvalidIndex;

        // Haptic type 2 is the ERM model: an amplitude and a brake flag per side, low frequency
        // on the left motor and high frequency on the right.
        uint8_t rumbleData[SINPUT_COMMAND_BUFFER_SIZE]{
            SINPUT_REPORT_ID_COMMAND,
            SINPUT_COMMAND_HAPTIC,
            SINPUT_HAPTIC_TYPE_RUMBLE,
            static_cast<uint8_t>(amp_low * 255),
            0x00,
            static_cast<uint8_t>(amp_high * 255),
            0x00};

        return m_outPipe[input_idx]->Write(rumbleData, sizeof(rumbleData));
    }

    size_t SInputController::GetMaxInputBufferSize()
    {
        return SINPUT_INPUT_BUFFER_SIZE;
    }
} // namespace controllerlib
