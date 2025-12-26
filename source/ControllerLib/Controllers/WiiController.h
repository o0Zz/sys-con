#pragma once

#include "BaseController.h"

#define WII_INPUT_BUFFER_SIZE 37
#define WII_MAX_INPUTS        4

class WiiController : public BaseController
{
protected:
    uint8_t m_current_wii_controller_idx = 0;
    uint8_t m_buffer[WII_INPUT_BUFFER_SIZE];
    bool m_is_connected[WII_MAX_INPUTS];
    bool m_rumble_supported[WII_MAX_INPUTS];
    uint8_t rumbleData[5] = {0x11, 0, 0, 0, 0};

    ControllerResult ReadNextBuffer(uint8_t *buffer, size_t *size, uint16_t *input_idx, uint32_t timeout_us) override;

public:
    WiiController(std::unique_ptr<IUSBDevice> &&device,
                  const ControllerConfig &config,
                  std::unique_ptr<ILogger> &&logger);

    virtual ~WiiController();

    ControllerResult Initialize() override;

    ControllerResult ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;

    bool Support(ControllerFeature feature) override;

    ControllerResult SetRumble(uint16_t input_idx, float amp_high, float amp_low) override;

    uint16_t GetInputCount() override;

    bool IsControllerConnected(uint16_t input_idx) override;
};