#pragma once

#include <switch.h>
#include "IKeyboard.h"
#include "SwitchVirtualDeviceHandler.h"
#include "config_handler.h"

// Publishes a USB keyboard to the console as a keyboard. Unlike a gamepad there is no
// attach/detach and no slot: the console has exactly one keyboard view, and the merge of
// several physical keyboards into it lives in syscon::hid::VirtualKeyboard.
class SwitchKeyboardHandler : public SwitchVirtualDeviceHandler
{
public:
    SwitchKeyboardHandler(std::unique_ptr<controllerlib::IKeyboard> &&keyboard, syscon::config::VirtualPadMode mode, int32_t polling_timeout_ms, int8_t thread_priority);
    ~SwitchKeyboardHandler() override;

    Result Initialize() override;
    void Exit() override;

    controllerlib::Status UpdateInput(uint32_t timeout_us) override;
    Result UpdateOutput() override;

    controllerlib::IUSBDevice *GetDevice() override { return m_keyboard->GetDevice(); }

protected:
    size_t GetInterfaceCount() override { return m_keyboard->GetDevice()->GetInterfaces().size(); }

private:
    std::unique_ptr<controllerlib::IKeyboard> m_keyboard;
    syscon::config::VirtualPadMode m_mode;
    int m_source = -1;
};
