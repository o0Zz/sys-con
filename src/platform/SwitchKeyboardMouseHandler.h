#pragma once

#include <switch.h>
#include "SwitchDeviceHandler.h"
#include "HidKeyboardMouse.h"
#include <memory>

/*
    A USB keyboard or mouse, read in the HID report protocol and decoded through its own report
    descriptor, then published through hiddbg's auto-pilot: hid writes the state into the
    keyboard or mouse section of its own shared memory, which mode=mitm mirrors into every fake
    one verbatim. One path for both modes.
*/
class SwitchKeyboardMouseHandler : public SwitchDeviceHandler
{
    friend void SwitchKeyboardMouseHandlerThreadFunc(void *arg);

    std::unique_ptr<controllerlib::IUSBDevice> m_device;
    controllerlib::IUSBEndpoint *m_endpoint = nullptr;
    int32_t m_polling_timeout_ms;
    int8_t m_thread_priority;

    alignas(0x1000) u8 m_thread_stack[0x2000];
    Thread m_thread{};
    bool m_thread_running = false;

    void Run();

protected:
    virtual controllerlib::Status OnDescriptor(const uint8_t *descriptor, size_t size) = 0;
    virtual void OnStart() = 0;
    virtual void OnReport(const uint8_t *report, size_t size) = 0;
    virtual void OnIdle() = 0;
    virtual void Release() = 0;

    // Derived destructors call it: Release() is virtual.
    void Exit();

    uint16_t GetVendor() { return m_device->GetVendor(); }
    uint16_t GetProduct() { return m_device->GetProduct(); }

public:
    SwitchKeyboardMouseHandler(std::unique_ptr<controllerlib::IUSBDevice> &&device, int32_t polling_timeout_ms, int8_t thread_priority);

    Result Initialize() override;
    controllerlib::IUSBDevice *GetDevice() override { return m_device.get(); }
};

class SwitchKeyboardHandler : public SwitchKeyboardMouseHandler
{
    controllerlib::KeyboardDecoder m_decoder;
    controllerlib::KeyboardState m_previous{};
    u64 m_lockModifiers = 0;
    bool m_published = false;

    void Publish(HiddbgKeyboardAutoPilotState state);

protected:
    controllerlib::Status OnDescriptor(const uint8_t *descriptor, size_t size) override { return m_decoder.Initialize(descriptor, size); }
    void OnStart() override { Publish({}); }
    void OnReport(const uint8_t *report, size_t size) override;
    void OnIdle() override {}
    void Release() override;

public:
    using SwitchKeyboardMouseHandler::SwitchKeyboardMouseHandler;
    ~SwitchKeyboardHandler() override;
};

class SwitchMouseHandler : public SwitchKeyboardMouseHandler
{
    controllerlib::MouseDecoder m_decoder;
    HiddbgMouseAutoPilotState m_state{};
    bool m_published = false;

    void Publish();

protected:
    controllerlib::Status OnDescriptor(const uint8_t *descriptor, size_t size) override { return m_decoder.Initialize(descriptor, size); }
    void OnStart() override { Publish(); }
    void OnReport(const uint8_t *report, size_t size) override;
    void OnIdle() override;
    void Release() override;

public:
    SwitchMouseHandler(std::unique_ptr<controllerlib::IUSBDevice> &&device, int32_t polling_timeout_ms, int8_t thread_priority);
    ~SwitchMouseHandler() override;
};
