#include "controller_handler.h"
#include <switch.h>

// Both handlers are compiled into both build flavours; the active one is chosen at
// runtime from the config `mode` (see SetMode / g_mode below).
#include "SwitchMITMHandler.h"
#include "SwitchHDLHandler.h"
#include "SwitchKeyboardMouseHandler.h"

#include "SwitchUSBInterface.h"
#include <mutex>

#include "logger.h"

using namespace controllerlib;

namespace syscon::controllers
{
    namespace
    {
        constexpr size_t MaxControllerHandlersSize = 10;
        std::vector<std::unique_ptr<SwitchDeviceHandler>> controllerHandlers;
        std::mutex controllerMutex;
        int32_t polling_timeout_ms = 0;
        int8_t polling_thread_priority = 0x30;
        config::VirtualPadMode virtual_pad_mode = config::VirtualPadMode::HIDDBG;

        Result Add(std::unique_ptr<SwitchDeviceHandler> &&handler)
        {
            Result rc = handler->Initialize();
            if (R_FAILED(rc))
            {
                syscon::logger::LogError("Controller[%04x-%04x] Failed to initialize controller: Error: 0x%X (Module: 0x%X, Desc: 0x%X)", handler->GetDevice()->GetVendor(), handler->GetDevice()->GetProduct(), rc, R_MODULE(rc), R_DESCRIPTION(rc));
                return rc;
            }

            syscon::logger::LogInfo("Controller[%04x-%04x] plugged !", handler->GetDevice()->GetVendor(), handler->GetDevice()->GetProduct());

            std::lock_guard<std::mutex> scoped_lock(controllerMutex);
            controllerHandlers.push_back(std::move(handler));
            return 0;
        }
    } // namespace

    bool IsAtControllerLimit()
    {
        std::lock_guard<std::mutex> scoped_lock(controllerMutex);
        return controllerHandlers.size() >= MaxControllerHandlersSize;
    }

    Result Insert(std::unique_ptr<IController> &&controllerPtr, bool removable)
    {
        std::unique_ptr<SwitchVirtualGamepadHandler> switchHandler;
        if (virtual_pad_mode == config::VirtualPadMode::MITM)
            switchHandler = std::make_unique<SwitchMITMHandler>(std::move(controllerPtr), polling_timeout_ms, polling_thread_priority);
        else
            switchHandler = std::make_unique<SwitchHDLHandler>(std::move(controllerPtr), polling_timeout_ms, polling_thread_priority);

        switchHandler->SetRemovable(removable);

        return Add(std::move(switchHandler));
    }

    Result InsertKeyboard(std::unique_ptr<IUSBDevice> &&device)
    {
        return Add(std::make_unique<SwitchKeyboardHandler>(std::move(device), polling_timeout_ms, polling_thread_priority));
    }

    Result InsertMouse(std::unique_ptr<IUSBDevice> &&device)
    {
        return Add(std::make_unique<SwitchMouseHandler>(std::move(device), polling_timeout_ms, polling_thread_priority));
    }

    void RemoveAllNonPlugged(const std::vector<s32> &interfaceIDsPlugged)
    {
        /*
            A handler's destructor joins its polling thread and tears down USB, so it must not
            run while controllerMutex is held. Move the unplugged handlers into this local
            vector under the lock and let it destroy them once the lock is released.
        */
        std::vector<std::unique_ptr<SwitchDeviceHandler>> unpluggedHandlers;

        {
            std::lock_guard<std::mutex> scoped_lock(controllerMutex);

            for (auto it = controllerHandlers.begin(); it != controllerHandlers.end();)
            {
                if (!(*it)->IsRemovable())
                {
                    ++it;
                    continue;
                }

                bool found = false;

                for (auto &&ptr : (*it)->GetDevice()->GetInterfaces())
                {
                    for (auto &&interfaceID : interfaceIDsPlugged)
                    {
                        if (static_cast<SwitchUSBInterface *>(ptr.get())->GetID() == interfaceID)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                // We check if a device was removed by comparing the controller's interfaces and the currently acquired interfaces
                // If we didn't find a single matching interface ID, we consider a controller removed
                if (found)
                {
                    ++it;
                    continue;
                }

                syscon::logger::LogInfo("Controller[%04x-%04x] unplugged !", (*it)->GetDevice()->GetVendor(), (*it)->GetDevice()->GetProduct());

                unpluggedHandlers.push_back(std::move(*it));
                it = controllerHandlers.erase(it);
            }
        }
    }

    void SetPollingParameters(int32_t _polling_timeout_ms, s8 _polling_thread_priority)
    {
        polling_timeout_ms = _polling_timeout_ms;
        polling_thread_priority = _polling_thread_priority;
    }

    void SetMode(config::VirtualPadMode mode)
    {
        virtual_pad_mode = mode;
    }

    void Initialize()
    {
        controllerHandlers.reserve(MaxControllerHandlersSize);
    }

    void Clear()
    {
        syscon::logger::LogDebug("Controllers clear (Release all controllers) !");
        std::lock_guard<std::mutex> scoped_lock(controllerMutex);
        controllerHandlers.clear();
    }
} // namespace syscon::controllers
