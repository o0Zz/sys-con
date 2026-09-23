#pragma once

#include "IController.h"
#include <vector>

namespace controllerlib
{
    class RawInputData
    {
    public:
        // Indexed by PinId (physical pin), not by GamepadButton.
        RawButtonStates buttons{};
        AnalogValues analog{};
    };

    class BaseController : public IController
    {
    protected:
        std::vector<IUSBEndpoint *> m_inPipe;
        std::vector<IUSBEndpoint *> m_outPipe;
        std::vector<IUSBInterface *> m_interfaces;
        uint8_t m_current_controller_idx = 0;

        virtual Status ReadNextBuffer(uint8_t *buffer, size_t *size, uint16_t *input_idx, uint32_t timeout_us);
        // Read the freshest report from a single endpoint, draining any already-queued reports (keep-latest).
        Status ReadEndpointLatest(uint16_t endpoint_idx, uint8_t *buffer, size_t *size, uint32_t timeout_us);
        virtual void MapRawInputToNormalized(RawInputData &rawData, NormalizedButtonData *normalData);

        virtual Status ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) = 0;

    public:
        BaseController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
        virtual ~BaseController() override;

        virtual Status Initialize() override;
        virtual void Exit() override;

        virtual Status OpenInterfaces();
        virtual void CloseInterfaces();

        bool Support(ControllerFeature feature) const override
        {
            return feature == SUPPORTS_RUMBLE && m_config.rumble.IsValid() && !m_outPipe.empty();
        }

        virtual uint16_t GetInputCount() override;

        Status ReadInput(NormalizedButtonData *normalData, uint16_t *input_idx, uint32_t timeout_us) override;

        Status SetRumble(uint16_t input_idx, float amp_high, float amp_low) override;

        virtual size_t GetMaxInputBufferSize();

        // Helper functions
        static uint32_t ScaleAmplitude(float amplitude, uint32_t max);
        static float Normalize(int32_t value, int32_t min, int32_t max);
        static float Normalize(int32_t value, int32_t min, int32_t max, int32_t center);
        static float ApplyDeadzone(uint8_t deadzonePercent, float value);
        static uint32_t ReadBitsLE(uint8_t *buffer, uint32_t bitOffset, uint32_t bitLength);
    };
} // namespace controllerlib
