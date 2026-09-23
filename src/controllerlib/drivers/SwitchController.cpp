#include "drivers/SwitchController.h"
#include <cstring>

#define SWITCH_INPUT_BUFFER_SIZE 64

#define SWITCH_OUTPUT_ID_SUBCOMMAND    0x01
#define SWITCH_OUTPUT_ID_RUMBLE        0x10
#define SWITCH_SUBCMD_ENABLE_IMU       0x40
#define SWITCH_SUBCMD_ENABLE_VIBRATION 0x48

namespace controllerlib
{
    static_assert(SWITCH_INPUT_BUFFER_SIZE == 64, "Input byte for switch as to be 64 bytes long");

    SwitchController::SwitchController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger))
    {
        cal_left_x.max = cal_left_y.max = cal_right_x.max = cal_right_y.max = 3400;
        cal_left_x.min = cal_left_y.min = cal_right_x.min = cal_right_y.min = 600;
    }

    SwitchController::~SwitchController()
    {
    }

    Status SwitchController::Initialize()
    {
        Status result = BaseController::Initialize();
        if (result != Status::Success)
            return result;

        if (m_outPipe.empty())
        {
            m_logger->Log(LogLevel::Error, "SwitchController: Initialization not complete ! No output endpoint found !");
            return Status::InvalidEndpoint;
        }

        /* The first transfer on the interface must be this OUT. Reading the IN endpoint before
           it (the old pre-handshake flush) makes usb:hs stall the very first OUT for ~33 s on
           pads that already stream 0x30 reports at enumeration (HOJA). Stale 81 xx / 0x30
           reports need no draining: ParseData discards anything that is not 0x30. */
        uint8_t initPacket1[SWITCH_INPUT_BUFFER_SIZE]{0x80, 0x02};
        (void)m_outPipe[0]->Write(initPacket1, sizeof(initPacket1));

        uint8_t buffer[SWITCH_INPUT_BUFFER_SIZE]{0x00};
        size_t size = sizeof(buffer);
        (void)m_inPipe[0]->Read(buffer, &size, 500 * 1000 /*timeout_us*/);

        // Forces the Joy-Con or Pro Controller to only talk over USB HID without any timeouts.
        uint8_t initPacket2_ForceToUSB[SWITCH_INPUT_BUFFER_SIZE]{0x80, 0x04};
        (void)m_outPipe[0]->Write(initPacket2_ForceToUSB, sizeof(initPacket2_ForceToUSB));

        size = sizeof(buffer);
        (void)m_inPipe[0]->Read(buffer, &size, 500 * 1000 /*timeout_us*/);

        // The motors ignore every rumble report until this subcommand turns them on.
        uint8_t enableVibration[]{
            SWITCH_OUTPUT_ID_SUBCOMMAND, (uint8_t)(m_packet_counter++ & 0x0F),
            0x00, 0x01, 0x40, 0x40,
            0x00, 0x01, 0x40, 0x40,
            SWITCH_SUBCMD_ENABLE_VIBRATION, 0x01};
        (void)m_outPipe[0]->Write(enableVibration, sizeof(enableVibration));

        size = sizeof(buffer);
        (void)m_inPipe[0]->Read(buffer, &size, 500 * 1000 /*timeout_us*/);

        // 0x30 reports carry zeroed IMU samples until this subcommand turns the sensor on.
        uint8_t enableIMU[]{
            SWITCH_OUTPUT_ID_SUBCOMMAND, (uint8_t)(m_packet_counter++ & 0x0F),
            0x00, 0x01, 0x40, 0x40,
            0x00, 0x01, 0x40, 0x40,
            SWITCH_SUBCMD_ENABLE_IMU, 0x01};
        (void)m_outPipe[0]->Write(enableIMU, sizeof(enableIMU));

        return Status::Success;
    }

    /*
        The amplitude the pad encodes is not linear in the step, so a requested amplitude has
        to be looked up rather than scaled. This is the amp column of joycon_rumble_amplitudes[]
        in https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c, in its own
        units, where full scale is 1003.
    */
    static constexpr uint16_t RUMBLE_AMPLITUDE_STEPS[]{
        0, 10, 12, 14, 17, 20, 24, 28, 33, 40,
        47, 56, 67, 80, 95, 112, 117, 123, 128, 134,
        140, 146, 152, 159, 166, 173, 181, 189, 198, 206,
        215, 225, 230, 235, 240, 245, 251, 256, 262, 268,
        273, 279, 286, 292, 298, 305, 311, 318, 325, 332,
        340, 347, 355, 362, 370, 378, 387, 395, 404, 413,
        422, 431, 440, 450, 460, 470, 480, 491, 501, 512,
        524, 535, 547, 559, 571, 584, 596, 609, 623, 636,
        650, 665, 679, 694, 709, 725, 741, 757, 773, 790,
        808, 825, 843, 862, 881, 900, 920, 940, 960, 981,
        1003};

    static constexpr uint16_t RUMBLE_AMPLITUDE_MAX = RUMBLE_AMPLITUDE_STEPS[(sizeof(RUMBLE_AMPLITUDE_STEPS) / sizeof(RUMBLE_AMPLITUDE_STEPS[0])) - 1];
    static constexpr uint32_t RUMBLE_AMPLITUDE_LAST_STEP = (sizeof(RUMBLE_AMPLITUDE_STEPS) / sizeof(RUMBLE_AMPLITUDE_STEPS[0])) - 1;

    /*
        An actuator takes an encoded frequency/amplitude pair. The frequencies stay at the
        defaults (160 Hz low, 320 Hz high), which is what makes the idle pair 00 01 40 40 and
        full scale 00 C9 40 72; only the amplitude moves. Ref: joycon_encode_rumble, same file.
    */
    void SwitchController::EncodeRumble(uint8_t *data, float amplitude)
    {
        const uint32_t wanted = ScaleAmplitude(amplitude, RUMBLE_AMPLITUDE_MAX);

        uint32_t step = 0;
        while (step < RUMBLE_AMPLITUDE_LAST_STEP && RUMBLE_AMPLITUDE_STEPS[step] < wanted)
            step++;

        const uint16_t amp_low = (uint16_t)(0x0040 + (step / 2) + ((step % 2) ? 0x8000 : 0x0000));

        data[0] = 0x00;
        data[1] = (uint8_t)(0x01 + (step * 2));
        data[2] = (uint8_t)(0x40 + (amp_low >> 8));
        data[3] = (uint8_t)(amp_low & 0xFF);
    }

    Status SwitchController::SetRumble(uint16_t input_idx, float amp_high, float amp_low)
    {
        if (input_idx != 0)
            return Status::InvalidIndex;

        if (m_outPipe.empty())
            return Status::InvalidEndpoint;

        uint8_t rumblePacket[10]{SWITCH_OUTPUT_ID_RUMBLE, (uint8_t)(m_packet_counter++ & 0x0F)};
        EncodeRumble(&rumblePacket[2], amp_low);
        EncodeRumble(&rumblePacket[6], amp_high);

        return m_outPipe[0]->Write(rumblePacket, sizeof(rumblePacket));
    }

    Status SwitchController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        (void)input_idx;
        SwitchButtonData *buttonData = reinterpret_cast<SwitchButtonData *>(buffer);

        if (size < SWITCH_IMU_OFFSET + SWITCH_IMU_SAMPLE_COUNT * sizeof(SwitchIMUSample))
            return Status::UnexpectedData;

        if (buttonData->report_id != 0x30)
            return Status::NothingTodo;

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
        rawData->buttons[14] = buttonData->button14;
        rawData->buttons[15] = buttonData->button15;
        rawData->buttons[16] = buttonData->button16;
        rawData->buttons[17] = buttonData->button17;
        rawData->buttons[18] = buttonData->button18;
        rawData->buttons[19] = buttonData->button19;

        uint16_t left_x = BaseController::ReadBitsLE(buttonData->stick_left, 0, 12);
        uint16_t left_y = BaseController::ReadBitsLE(buttonData->stick_left, 12, 12);
        uint16_t right_x = BaseController::ReadBitsLE(buttonData->stick_right, 0, 12);
        uint16_t right_y = BaseController::ReadBitsLE(buttonData->stick_right, 12, 12);

        cal_left_x.max = std::max(left_x, cal_left_x.max);
        cal_left_x.min = std::min(left_x, cal_left_x.min);
        cal_left_y.max = std::max(left_y, cal_left_y.max);
        cal_left_y.min = std::min(left_y, cal_left_y.min);
        cal_right_x.max = std::max(right_x, cal_right_x.max);
        cal_right_x.min = std::min(right_x, cal_right_x.min);
        cal_right_y.max = std::max(right_y, cal_right_y.max);
        cal_right_y.min = std::min(right_y, cal_right_y.min);

        m_logger->Log(LogLevel::Trace, "X=%u, Y=%u, Z=%u, Rz=%u (Calib: X=[%u,%u], Y=[%u,%u], Z=[%u,%u], Rz=[%u,%u])",
                      left_x, left_y, right_x, right_y,
                      cal_left_x.min, cal_left_x.max, cal_left_y.min, cal_left_y.max,
                      cal_right_x.min, cal_right_x.max, cal_right_y.min, cal_right_y.max);

        rawData->analog[AnalogAxis::X] = BaseController::Normalize(left_x, cal_left_x.min, cal_left_x.max, 2000);
        rawData->analog[AnalogAxis::Y] = -1.0f * BaseController::Normalize(left_y, cal_left_y.min, cal_left_y.max, 2000);
        rawData->analog[AnalogAxis::Z] = BaseController::Normalize(right_x, cal_right_x.min, cal_right_x.max, 2000);
        rawData->analog[AnalogAxis::Rz] = -1.0f * BaseController::Normalize(right_y, cal_right_y.min, cal_right_y.max, 2000);

        rawData->buttons[DPAD_UP_BUTTON_ID] = buttonData->dpad_up;
        rawData->buttons[DPAD_RIGHT_BUTTON_ID] = buttonData->dpad_right;
        rawData->buttons[DPAD_DOWN_BUTTON_ID] = buttonData->dpad_down;
        rawData->buttons[DPAD_LEFT_BUTTON_ID] = buttonData->dpad_left;

        SwitchIMUSample newest;
        memcpy(&newest, buffer + SWITCH_IMU_OFFSET, sizeof(newest));

        // Nominal LSB sizes (SDL's SWITCH_ACCEL_SCALE / SWITCH_GYRO_SCALE); the axes are
        // reordered to SDL's frame the way SDL does for the Pro Controller.
        constexpr float AccelScale = StandardGravity / 4096.0f;
        constexpr float GyroScale = RadiansPerDegree / 14.2842f;
        rawData->motion.accel[0] = -newest.accel_y * AccelScale;
        rawData->motion.accel[1] = newest.accel_z * AccelScale;
        rawData->motion.accel[2] = -newest.accel_x * AccelScale;
        rawData->motion.gyro[0] = -newest.gyro_y * GyroScale;
        rawData->motion.gyro[1] = newest.gyro_z * GyroScale;
        rawData->motion.gyro[2] = -newest.gyro_x * GyroScale;

        return Status::Success;
    }

    size_t SwitchController::GetMaxInputBufferSize()
    {
        return SWITCH_INPUT_BUFFER_SIZE;
    }
} // namespace controllerlib
