#pragma once

#include "IMouse.h"
#include "drivers/UsbPipeSet.h"

#include <memory>

class HIDMouse;
class HIDReportDescriptor;

namespace controllerlib
{
    class HIDMouseController : public IMouse, protected UsbPipeSet
    {
    public:
        HIDMouseController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
        ~HIDMouseController() override;

        Status Initialize() override;
        void Exit() override;

        Status ReadInput(MouseState *state, uint32_t timeout_us) override;

    private:
        std::shared_ptr<HIDReportDescriptor> m_descriptor;
        std::shared_ptr<HIDMouse> m_mouse;

        Status Accumulate(uint8_t *buffer, size_t size, MouseState *state);
    };
} // namespace controllerlib
