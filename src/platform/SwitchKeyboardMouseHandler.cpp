#include "SwitchKeyboardMouseHandler.h"
#include "HorizonResult.h"
#include "SwitchLogger.h"
#include "SwitchUSBInterface.h"
#include "ControllerTypes.h"
#include <algorithm>

using namespace controllerlib;

namespace
{
    constexpr s32 ScreenWidth = 1280;
    constexpr s32 ScreenHeight = 720;

    /*
        hid's keyboard state holds a u32 modifier word followed by a u32 attribute word; the
        auto-pilot u64 "modifiers" covers both. Without IsConnected, hid marks every all-zero
        state disconnected, the software keyboard skips those samples, never sees a key go up,
        and ignores the same key pressed again.
    */
    constexpr u64 KeyboardAttributeIsConnected = u64{1} << 32;

    constexpr struct
    {
        u8 usb_mask;
        u64 modifier;
    } KeyboardModifiers[] = {
        {0x11, HidKeyboardModifier_Control},
        {0x22, HidKeyboardModifier_Shift},
        {0x04, HidKeyboardModifier_LeftAlt},
        {0x40, HidKeyboardModifier_RightAlt},
        {0x88, HidKeyboardModifier_Gui},
    };

    // hid reports lock state, not the key: it toggles on each press.
    constexpr struct
    {
        u8 usage;
        u64 modifier;
    } LockKeys[] = {
        {HidKeyboardKey_CapsLock, HidKeyboardModifier_CapsLock},
        {HidKeyboardKey_ScrollLock, HidKeyboardModifier_ScrollLock},
        {HidKeyboardKey_NumLock, HidKeyboardModifier_NumLock},
    };

    constexpr struct
    {
        u8 usb_mask;
        u32 button;
    } MouseButtons[] = {
        {0x01, HidMouseButton_Left},
        {0x02, HidMouseButton_Right},
        {0x04, HidMouseButton_Middle},
        {0x08, HidMouseButton_Back},
        {0x10, HidMouseButton_Forward},
    };
} // namespace

void SwitchKeyboardMouseHandlerThreadFunc(void *handler)
{
    static_cast<SwitchKeyboardMouseHandler *>(handler)->Run();
}

SwitchKeyboardMouseHandler::SwitchKeyboardMouseHandler(std::unique_ptr<IUSBDevice> &&device, int32_t polling_timeout_ms, int8_t thread_priority)
    : m_device(std::move(device)),
      m_polling_timeout_ms(polling_timeout_ms),
      m_thread_priority(thread_priority)
{
}

Result SwitchKeyboardMouseHandler::Initialize()
{
    Status status = m_device->Open();
    if (Failed(status))
        return syscon::ToHorizonResult(status);

    IUSBInterface *interface = m_device->GetInterfaces().front().get();

    status = interface->Open();
    if (Failed(status))
        return syscon::ToHorizonResult(status);

    const uint16_t interfaceNumber = interface->GetDescriptor()->bInterfaceNumber;

    status = interface->ControlTransferOutput(HidRequestTypeClassInterfaceOut, HidRequestSetProtocol, HidReportProtocol, interfaceNumber, nullptr, 0);
    if (Failed(status))
    {
        syscon::logger::LogError("SwitchKeyboardMouseHandler[%04x-%04x] SET_PROTOCOL(report) refused: %s", GetVendor(), GetProduct(), ToString(status));
        return syscon::ToHorizonResult(status);
    }

    uint8_t descriptor[CONTROLLER_HID_REPORT_BUFFER_SIZE];
    uint16_t descriptorSize = sizeof(descriptor);
    status = interface->ControlTransferInput(HidRequestTypeStandardInterfaceIn, HidRequestGetDescriptor, HidReportDescriptorValue, interfaceNumber, descriptor, &descriptorSize);
    if (Failed(status))
    {
        syscon::logger::LogError("SwitchKeyboardMouseHandler[%04x-%04x] Failed to get HID report descriptor: %s", GetVendor(), GetProduct(), ToString(status));
        return syscon::ToHorizonResult(status);
    }

    syscon::logger::LogDebug("SwitchKeyboardMouseHandler[%04x-%04x] HID report descriptor (%d bytes):", GetVendor(), GetProduct(), static_cast<int>(descriptorSize));
    syscon::logger::LogBuffer(LogLevel::Debug, descriptor, descriptorSize);

    status = OnDescriptor(descriptor, descriptorSize);
    if (Failed(status))
    {
        syscon::logger::LogError("SwitchKeyboardMouseHandler[%04x-%04x] Unusable HID report descriptor: %s", GetVendor(), GetProduct(), ToString(status));
        return syscon::ToHorizonResult(status);
    }

    for (uint8_t idx = 0; idx < SWITCH_USB_MAX_ENDPOINTS && m_endpoint == nullptr; idx++)
        m_endpoint = interface->GetEndpoint(IUSBEndpoint::USB_ENDPOINT_IN, idx);

    if (m_endpoint == nullptr)
    {
        syscon::logger::LogError("SwitchKeyboardMouseHandler[%04x-%04x] No input endpoint", GetVendor(), GetProduct());
        return syscon::ToHorizonResult(Status::InvalidEndpoint);
    }

    status = m_endpoint->Open();
    if (Failed(status))
        return syscon::ToHorizonResult(status);

    m_thread_running = true;
    Result rc = threadCreate(&m_thread, &SwitchKeyboardMouseHandlerThreadFunc, this, m_thread_stack, sizeof(m_thread_stack), m_thread_priority, 3 /* On CPU 3 responsible for input */);
    if (R_FAILED(rc))
        return rc;

    return threadStart(&m_thread);
}

void SwitchKeyboardMouseHandler::Exit()
{
    if (m_thread.handle != INVALID_HANDLE)
    {
        m_thread_running = false;
        svcCancelSynchronization(m_thread.handle);
        threadWaitForExit(&m_thread);
        threadClose(&m_thread);
    }

    m_device->Close();
    Release();
}

void SwitchKeyboardMouseHandler::Run()
{
    uint8_t report[64];

    OnStart();

    while (m_thread_running)
    {
        size_t size = sizeof(report);
        Status status = m_endpoint->Read(report, &size, m_polling_timeout_ms * 1000);

        if (status == Status::Success)
            OnReport(report, size);
        else if (status == Status::Timeout)
            OnIdle();
        else if (status != Status::NoDataAvailable)
            svcSleepThread(100000000); // Unplugged: leave the CPU to the thread that notices it.
    }
}

SwitchKeyboardHandler::~SwitchKeyboardHandler()
{
    Exit();
}

void SwitchKeyboardHandler::OnReport(const uint8_t *report, size_t size)
{
    KeyboardState state;
    if (Failed(m_decoder.Parse(report, size, &state)))
        return;

    for (const auto &lock : LockKeys)
    {
        if (state.IsPressed(lock.usage) && !m_previous.IsPressed(lock.usage))
            m_lockModifiers ^= lock.modifier;
    }
    m_previous = state;

    HiddbgKeyboardAutoPilotState autopilot{};
    autopilot.modifiers = m_lockModifiers;
    for (const auto &modifier : KeyboardModifiers)
    {
        if (state.modifiers & modifier.usb_mask)
            autopilot.modifiers |= modifier.modifier;
    }
    std::copy(state.keys.begin(), state.keys.end(), autopilot.keys);

    Publish(autopilot);
}

void SwitchKeyboardHandler::Publish(HiddbgKeyboardAutoPilotState state)
{
    state.modifiers |= KeyboardAttributeIsConnected;

    Result rc = hiddbgSetKeyboardAutoPilotState(&state);
    if (R_FAILED(rc))
    {
        syscon::logger::LogError("SwitchKeyboardHandler[%04x-%04x] hiddbgSetKeyboardAutoPilotState failed: 0x%08X", GetVendor(), GetProduct(), rc);
        return;
    }

    m_published = true;
    syscon::logger::LogDebug("SwitchKeyboardHandler[%04x-%04x] Modifiers: 0x%04llX Keys: %016llX %016llX %016llX %016llX", GetVendor(), GetProduct(), (unsigned long long)state.modifiers, (unsigned long long)state.keys[0], (unsigned long long)state.keys[1], (unsigned long long)state.keys[2], (unsigned long long)state.keys[3]);
}

void SwitchKeyboardHandler::Release()
{
    if (m_published)
        hiddbgUnsetKeyboardAutoPilotState();
}

SwitchMouseHandler::SwitchMouseHandler(std::unique_ptr<IUSBDevice> &&device, int32_t polling_timeout_ms, int8_t thread_priority)
    : SwitchKeyboardMouseHandler(std::move(device), polling_timeout_ms, thread_priority)
{
    m_state.x = ScreenWidth / 2;
    m_state.y = ScreenHeight / 2;
    m_state.attributes = HidMouseAttribute_IsConnected;
}

SwitchMouseHandler::~SwitchMouseHandler()
{
    Exit();
}

void SwitchMouseHandler::OnReport(const uint8_t *report, size_t size)
{
    MouseReport mouse;
    if (Failed(m_decoder.Parse(report, size, &mouse)))
        return;

    m_state.buttons = 0;
    for (const auto &button : MouseButtons)
    {
        if (mouse.buttons & button.usb_mask)
            m_state.buttons |= button.button;
    }

    m_state.delta_x = mouse.delta_x;
    m_state.delta_y = mouse.delta_y;
    m_state.x = std::clamp(m_state.x + mouse.delta_x, 0, ScreenWidth - 1);
    m_state.y = std::clamp(m_state.y + mouse.delta_y, 0, ScreenHeight - 1);
    m_state.wheel_delta = mouse.wheel;

    Publish();
}

/*
    hid copies the auto-pilot state into every sample it takes, deltas included, so a delta
    left in place keeps the cursor moving. A mouse reports nothing while it is still, which is
    when it gets cleared.
*/
void SwitchMouseHandler::OnIdle()
{
    if (m_state.delta_x == 0 && m_state.delta_y == 0 && m_state.wheel_delta == 0)
        return;

    m_state.delta_x = 0;
    m_state.delta_y = 0;
    m_state.wheel_delta = 0;
    Publish();
}

void SwitchMouseHandler::Publish()
{
    Result rc = hiddbgSetMouseAutoPilotState(&m_state);
    if (R_FAILED(rc))
    {
        syscon::logger::LogError("SwitchMouseHandler[%04x-%04x] hiddbgSetMouseAutoPilotState failed: 0x%08X", GetVendor(), GetProduct(), rc);
        return;
    }

    m_published = true;
    syscon::logger::LogTrace("SwitchMouseHandler[%04x-%04x] Pos: %d,%d Delta: %d,%d Wheel: %d Buttons: 0x%X", GetVendor(), GetProduct(), m_state.x, m_state.y, m_state.delta_x, m_state.delta_y, m_state.wheel_delta, m_state.buttons);
}

void SwitchMouseHandler::Release()
{
    if (m_published)
        hiddbgUnsetMouseAutoPilotState();
}
