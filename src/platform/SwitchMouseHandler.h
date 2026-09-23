#pragma once

#include <switch.h>
#include "IMouse.h"
#include "SwitchVirtualDeviceHandler.h"
#include "config_handler.h"

class SwitchMouseHandler : public SwitchVirtualDeviceHandler
{
public:
    SwitchMouseHandler(std::unique_ptr<controllerlib::IMouse> &&mouse, syscon::config::VirtualPadMode mode, int32_t polling_timeout_ms, int8_t thread_priority);
    ~SwitchMouseHandler() override;

    Result Initialize() override;
    void Exit() override;

    controllerlib::Status UpdateInput(uint32_t timeout_us) override;
    Result UpdateOutput() override;

    controllerlib::IUSBDevice *GetDevice() override { return m_mouse->GetDevice(); }

protected:
    size_t GetInterfaceCount() override { return m_mouse->GetDevice()->GetInterfaces().size(); }

private:
    std::unique_ptr<controllerlib::IMouse> m_mouse;
    syscon::config::VirtualPadMode m_mode;
    int m_source = -1;
};
