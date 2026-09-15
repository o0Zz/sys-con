#include "drivers/NetworkController.h"

#include <cstring>

namespace controllerlib
{
    NetworkController::NetworkController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger))
    {
    }

    NetworkController::~NetworkController()
    {
    }

    size_t NetworkController::GetMaxInputBufferSize()
    {
        return sizeof(NetworkPadReport);
    }

    Status NetworkController::ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx)
    {
        (void)input_idx;

        if (size < sizeof(NetworkPadReport))
            return Status::UnexpectedData;

        /*
            memcpy rather than a reinterpret_cast: the report arrives in a plain uint8_t buffer
            and this driver, unlike the hardware ones, is compiled for the host test build too,
            where the unaligned access a cast would allow is not guaranteed to be benign.
        */
        NetworkPadReport report;
        memcpy(&report, buffer, sizeof(report));

        if (report.magic != NetworkPadReportMagic || report.version != NetworkPadReportVersion)
            return Status::UnexpectedData;

        m_connected = report.connected != 0;

        // Identity mapping: bit N is GamepadButton N, and lands on pin N. Bit 0 is NONE, which
        // is also PinId::Unmapped, so it is deliberately skipped at both ends.
        for (uint8_t pin = 1; pin < GamepadButtonCount; pin++)
            rawData->buttons[pin] = ((report.buttons >> pin) & 1u) != 0;

        rawData->analog[AnalogAxis::X] = BaseController::Normalize(report.stick_left_x, -32768, 32767);
        rawData->analog[AnalogAxis::Y] = BaseController::Normalize(report.stick_left_y, -32768, 32767);
        rawData->analog[AnalogAxis::Z] = BaseController::Normalize(report.stick_right_x, -32768, 32767);
        rawData->analog[AnalogAxis::Rz] = BaseController::Normalize(report.stick_right_y, -32768, 32767);

        return Status::Success;
    }

    bool NetworkController::IsControllerConnected(uint16_t input_idx)
    {
        (void)input_idx;
        return m_connected;
    }

    bool NetworkController::Support(ControllerFeature feature)
    {
        (void)feature;
        return false;
    }

    Status NetworkController::SetRumble(uint16_t input_idx, float amp_high, float amp_low)
    {
        (void)input_idx;
        (void)amp_high;
        (void)amp_low;
        // Input-only by design: there is no return path to the sender.
        return Status::NotImplemented;
    }
} // namespace controllerlib
