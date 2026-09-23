#include "SwitchKeyboardHandler.h"
#include "SwitchVirtualHid.h"
#include "SwitchAutoPilotHid.h"
#include "SwitchMITMManager.h"
#include "HorizonResult.h"
#include "SwitchLogger.h"

using namespace controllerlib;

SwitchKeyboardHandler::SwitchKeyboardHandler(std::unique_ptr<IKeyboard> &&keyboard, syscon::config::VirtualPadMode mode, int32_t polling_timeout_ms, int8_t thread_priority)
    : SwitchVirtualDeviceHandler(polling_timeout_ms, thread_priority),
      m_keyboard(std::move(keyboard)),
      m_mode(mode)
{
}

SwitchKeyboardHandler::~SwitchKeyboardHandler()
{
    Exit();
}

Result SwitchKeyboardHandler::Initialize()
{
    syscon::logger::LogDebug("SwitchKeyboardHandler[%04x-%04x] Initializing ...", m_keyboard->GetDevice()->GetVendor(), m_keyboard->GetDevice()->GetProduct());

    Status status = m_keyboard->Initialize();
    if (Failed(status))
    {
        syscon::logger::LogError("SwitchKeyboardHandler[%04x-%04x] Keyboard initialization failed: %s", m_keyboard->GetDevice()->GetVendor(), m_keyboard->GetDevice()->GetProduct(), ToString(status));
        return syscon::ToHorizonResult(status);
    }

    m_source = syscon::hid::VirtualKeyboard::Get().Acquire();
    if (m_source < 0)
    {
        syscon::logger::LogError("SwitchKeyboardHandler[%04x-%04x] No free keyboard slot !", m_keyboard->GetDevice()->GetVendor(), m_keyboard->GetDevice()->GetProduct());
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    Result rc = (m_mode == syscon::config::VirtualPadMode::MITM)
                    ? HidSharedMemoryManager::GetHidSharedMemoryManager().AttachKeyboard()
                    : syscon::hid::autopilot::AcquireKeyboard();
    if (R_FAILED(rc))
    {
        syscon::hid::VirtualKeyboard::Get().Release(m_source);
        m_source = -1;
        return rc;
    }

    return InitThread();
}

void SwitchKeyboardHandler::Exit()
{
    uint16_t vendor = m_keyboard->GetDevice()->GetVendor();
    uint16_t product = m_keyboard->GetDevice()->GetProduct();

    syscon::logger::LogDebug("SwitchKeyboardHandler[%04x-%04x] Exiting ...", vendor, product);

    ExitThread();
    m_keyboard->Exit();

    if (m_source < 0)
        return;

    if (m_mode == syscon::config::VirtualPadMode::MITM)
        HidSharedMemoryManager::GetHidSharedMemoryManager().DetachKeyboard();
    else
        syscon::hid::autopilot::ReleaseKeyboard();

    syscon::hid::VirtualKeyboard::Get().Release(m_source);
    m_source = -1;

    syscon::logger::LogInfo("SwitchKeyboardHandler[%04x-%04x] Uninitialized !", vendor, product);
}

Status SwitchKeyboardHandler::UpdateInput(uint32_t timeout_us)
{
    KeyboardState state{};

    Status rc = m_keyboard->ReadInput(&state, timeout_us);
    if (rc != Status::Success)
        return rc;

    syscon::hid::VirtualKeyboard::Get().Update(m_source, state);
    return Status::Success;
}

Result SwitchKeyboardHandler::UpdateOutput()
{
    return 0;
}
