#include "drivers/SInputController.h"
#include <cstring>

namespace controllerlib
{
    SInputController::SInputController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger))
    {
    }

    SInputController::~SInputController()
    {
    }

    /*
        The IMU full-scale ranges are only known from the features reply, so motion stays off
        until it arrives. The reply is queued behind whatever input reports the pad already
        streams, which is why it is drained through ParseData like any other report.
    */
    Status SInputController::Initialize()
    {
        Status result = BaseController::Initialize();
        if (result != Status::Success)
            return result;

        if (m_outPipe.empty() || m_inPipe.empty())
            return Status::InvalidEndpoint;

        uint8_t featuresCommand[SINPUT_COMMAND_BUFFER_SIZE]{SINPUT_REPORT_ID_COMMAND, SINPUT_COMMAND_FEATURES};
        (void)m_outPipe[0]->Write(featuresCommand, sizeof(featuresCommand));

        for (int read = 0; read < 100 && !m_features_received; read++)
        {
            uint8_t buffer[SINPUT_INPUT_BUFFER_SIZE];
            size_t size = sizeof(buffer);
            RawInputData rawData;
            uint16_t input_idx = 0;
            if (m_inPipe[0]->Read(buffer, &size, 10 * 1000) == Status::Success)
                (void)ParseData(buffer, size, &rawData, &input_idx);
        }

        if (!m_features_received)
            m_logger->Log(LogLevel::Error, "SInputController[%04x-%04x] No features reply, motion disabled", m_device->GetVendor(), m_device->GetProduct());

        return Status::Success;
    }

    Status SInputController::ParseFeatures(const uint8_t *buffer, size_t size)
    {
        if (size < 2 + sizeof(SInputFeatures))
            return Status::UnexpectedData;

        SInputFeatures features;
        memcpy(&features, buffer + 2, sizeof(features));

        m_features_received = true;
        m_motion_supported = (features.feature_flags_0 & (SINPUT_FEATURE_ACCELEROMETER | SINPUT_FEATURE_GYROSCOPE)) != 0;
        m_accel_scale = StandardGravity * features.accel_range_g / 32768.0f;
        m_gyro_scale = RadiansPerDegree * features.gyro_range_dps / 32768.0f;

        m_logger->Log(LogLevel::Info, "SInputController[%04x-%04x] Features: motion %s (accel +/-%dg, gyro +/-%d dps)",
                      m_device->GetVendor(), m_device->GetProduct(), m_motion_supported ? "on" : "off", features.accel_range_g, features.gyro_range_dps);

        return Status::NothingTodo;
    }

    Status SInputController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        (void)input_idx;

        if (size < 2)
            return Status::UnexpectedData;

        if (buffer[0] == SINPUT_REPORT_ID_REPLY && buffer[1] == SINPUT_COMMAND_FEATURES)
            return ParseFeatures(buffer, size);

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

        if (m_motion_supported)
        {
            rawData->motion.accel[0] = -buttonData->accel_x * m_accel_scale;
            rawData->motion.accel[1] = buttonData->accel_z * m_accel_scale;
            rawData->motion.accel[2] = -buttonData->accel_y * m_accel_scale;
            rawData->motion.gyro[0] = -buttonData->gyro_x * m_gyro_scale;
            rawData->motion.gyro[1] = buttonData->gyro_z * m_gyro_scale;
            rawData->motion.gyro[2] = -buttonData->gyro_y * m_gyro_scale;
        }

        return Status::Success;
    }

    Status SInputController::SetRumble(uint16_t input_idx, const RumbleValue &rumble)
    {
        if (m_outPipe.size() <= input_idx)
            return Status::InvalidIndex;

        // Haptic type 2 is the ERM model: an amplitude and a brake flag per side, low frequency
        // on the left motor and high frequency on the right.
        uint8_t rumbleData[SINPUT_COMMAND_BUFFER_SIZE]{
            SINPUT_REPORT_ID_COMMAND,
            SINPUT_COMMAND_HAPTIC,
            SINPUT_HAPTIC_TYPE_RUMBLE,
            static_cast<uint8_t>(ScaleAmplitude(rumble.LowAmplitude(), 255)),
            0x00,
            static_cast<uint8_t>(ScaleAmplitude(rumble.HighAmplitude(), 255)),
            0x00};

        return m_outPipe[input_idx]->Write(rumbleData, sizeof(rumbleData));
    }

    size_t SInputController::GetMaxInputBufferSize()
    {
        return SINPUT_INPUT_BUFFER_SIZE;
    }
} // namespace controllerlib
