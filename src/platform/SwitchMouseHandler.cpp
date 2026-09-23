#include "SwitchMouseHandler.h"
#include "SwitchVirtualHid.h"
#include "SwitchAutoPilotHid.h"
#include "SwitchMITMManager.h"
#include "HorizonResult.h"
#include "SwitchLogger.h"

using namespace controllerlib;

SwitchMouseHandler::SwitchMouseHandler(std::unique_ptr<IMouse> &&mouse, syscon::config::VirtualPadMode mode, int32_t polling_timeout_ms, int8_t thread_priority)
    : SwitchVirtualDeviceHandler(polling_timeout_ms, thread_priority),
      m_mouse(std::move(mouse)),
      m_mode(mode)
{
}

SwitchMouseHandler::~SwitchMouseHandler()
{
    Exit();
}

Result SwitchMouseHandler::Initialize()
{
    syscon::logger::LogDebug("SwitchMouseHandler[%04x-%04x] Initializing ...", m_mouse->GetDevice()->GetVendor(), m_mouse->GetDevice()->GetProduct());

    Status status = m_mouse->Initialize();
    if (Failed(status))
    {
        syscon::logger::LogError("SwitchMouseHandler[%04x-%04x] Mouse initialization failed: %s", m_mouse->GetDevice()->GetVendor(), m_mouse->GetDevice()->GetProduct(), ToString(status));
        return syscon::ToHorizonResult(status);
    }

    m_source = syscon::hid::VirtualMouse::Get().Acquire();
    if (m_source < 0)
    {
        syscon::logger::LogError("SwitchMouseHandler[%04x-%04x] No free mouse slot !", m_mouse->GetDevice()->GetVendor(), m_mouse->GetDevice()->GetProduct());
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    Result rc = (m_mode == syscon::config::VirtualPadMode::MITM)
                    ? HidSharedMemoryManager::GetHidSharedMemoryManager().AttachMouse()
                    : syscon::hid::autopilot::AcquireMouse();
    if (R_FAILED(rc))
    {
        syscon::hid::VirtualMouse::Get().Release(m_source);
        m_source = -1;
        return rc;
    }

    return InitThread();
}

void SwitchMouseHandler::Exit()
{
    uint16_t vendor = m_mouse->GetDevice()->GetVendor();
    uint16_t product = m_mouse->GetDevice()->GetProduct();

    syscon::logger::LogDebug("SwitchMouseHandler[%04x-%04x] Exiting ...", vendor, product);

    ExitThread();
    m_mouse->Exit();

    if (m_source < 0)
        return;

    if (m_mode == syscon::config::VirtualPadMode::MITM)
        HidSharedMemoryManager::GetHidSharedMemoryManager().DetachMouse();
    else
        syscon::hid::autopilot::ReleaseMouse();

    syscon::hid::VirtualMouse::Get().Release(m_source);
    m_source = -1;

    syscon::logger::LogInfo("SwitchMouseHandler[%04x-%04x] Uninitialized !", vendor, product);
}

Status SwitchMouseHandler::UpdateInput(uint32_t timeout_us)
{
    MouseState state{};

    Status rc = m_mouse->ReadInput(&state, timeout_us);
    if (rc != Status::Success)
        return rc;

    syscon::hid::VirtualMouse::Get().Accumulate(m_source, state);
    return Status::Success;
}

Result SwitchMouseHandler::UpdateOutput()
{
    return 0;
}
