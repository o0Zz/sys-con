#include "switch.h"
#include "controller_handler.h"
#include "SwitchHDLHandler.h"
#include "SwitchMITMHandler.h"
#include "SwitchUSBInterface.h"
#include <algorithm>
#include <functional>
#include <mutex>

#include "logger.h"

namespace syscon::controllers
{
    namespace
    {
        constexpr size_t MaxControllerHandlersSize = 10;
        std::vector<std::unique_ptr<SwitchVirtualGamepadHandler>> controllerHandlers;
        std::mutex controllerMutex;
        int32_t polling_timeout_ms = 0;
        int8_t polling_thread_priority = 0x30;

    } // namespace

    bool IsAtControllerLimit()
    {
        std::lock_guard<std::mutex> scoped_lock(controllerMutex);
        return controllerHandlers.size() >= MaxControllerHandlersSize;
    }

    Result Insert(std::unique_ptr<IController> &&controllerPtr)
    {
        std::unique_ptr<SwitchVirtualGamepadHandler> switchHandler = std::make_unique<SwitchMITMHandler>(std::move(controllerPtr), polling_timeout_ms, polling_thread_priority);

        Result rc = switchHandler->Initialize();
        if (R_SUCCEEDED(rc))
        {
            syscon::logger::LogInfo("Controller[%04x-%04x] plugged !", switchHandler->GetController()->GetDevice()->GetVendor(), switchHandler->GetController()->GetDevice()->GetProduct());

            std::lock_guard<std::mutex> scoped_lock(controllerMutex);
            controllerHandlers.push_back(std::move(switchHandler));
        }
        else
        {
            syscon::logger::LogError("Controller[%04x-%04x] Failed to initialize controller: Error: 0x%X (Module: 0x%X, Desc: 0x%X)", switchHandler->GetController()->GetDevice()->GetVendor(), switchHandler->GetController()->GetDevice()->GetProduct(), rc, R_MODULE(rc), R_DESCRIPTION(rc));
        }

        return rc;
    }

    void RemoveAllNonPlugged(std::vector<s32> interfaceIDsPlugged)
    {
        std::lock_guard<std::mutex> scoped_lock(controllerMutex);
        for (auto it = controllerHandlers.begin(); it != controllerHandlers.end(); it++)
        {
            bool found = false;

            for (auto &&ptr : (*it)->GetController()->GetDevice()->GetInterfaces())
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
            if (!found)
            {
                syscon::logger::LogInfo("Controller[%04x-%04x] unplugged !", (*it)->GetController()->GetDevice()->GetVendor(), (*it)->GetController()->GetDevice()->GetProduct());
                controllerHandlers.erase(it--);
            }
        }
    }

    void SetPollingParameters(int32_t _polling_timeout_ms, s8 _polling_thread_priority)
    {
        polling_timeout_ms = _polling_timeout_ms;
        polling_thread_priority = _polling_thread_priority;
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

    void Exit()
    {
        Clear();
    }
} // namespace syscon::controllers
