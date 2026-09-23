#include "controller_handler.h"
#include <switch.h>

// Both handlers are compiled into both build flavours; the active one is chosen at
// runtime from the config `mode` (see SetMode / g_mode below).
#include "SwitchMITMHandler.h"
#include "SwitchHDLHandler.h"
#include "SwitchKeyboardHandler.h"
#include "SwitchMouseHandler.h"

#include "SwitchUSBInterface.h"
#include <algorithm>
#include <functional>
#include <mutex>

#include "logger.h"

using namespace controllerlib;

namespace syscon::controllers
{
    namespace
    {
        constexpr size_t MaxControllerHandlersSize = 10;
        std::vector<std::unique_ptr<SwitchVirtualDeviceHandler>> controllerHandlers;
        std::mutex controllerMutex;
        int32_t polling_timeout_ms = 0;
        int8_t polling_thread_priority = 0x30;
        config::VirtualPadMode virtual_pad_mode = config::VirtualPadMode::HIDDBG;

    } // namespace

    bool IsAtControllerLimit()
    {
        std::lock_guard<std::mutex> scoped_lock(controllerMutex);
        return controllerHandlers.size() >= MaxControllerHandlersSize;
    }

    namespace
    {
        Result InsertHandler(std::unique_ptr<SwitchVirtualDeviceHandler> &&switchHandler, bool removable)
        {
            switchHandler->SetRemovable(removable);

            const uint16_t vendor = switchHandler->GetDevice()->GetVendor();
            const uint16_t product = switchHandler->GetDevice()->GetProduct();

            Result rc = switchHandler->Initialize();
            if (R_SUCCEEDED(rc))
            {
                syscon::logger::LogInfo("Device[%04x-%04x] plugged !", vendor, product);

                std::lock_guard<std::mutex> scoped_lock(controllerMutex);
                controllerHandlers.push_back(std::move(switchHandler));
            }
            else
            {
                syscon::logger::LogError("Device[%04x-%04x] Failed to initialize device: Error: 0x%X (Module: 0x%X, Desc: 0x%X)", vendor, product, rc, R_MODULE(rc), R_DESCRIPTION(rc));
            }

            return rc;
        }
    } // namespace

    Result Insert(std::unique_ptr<IGamepad> &&gamepadPtr, bool removable)
    {
        std::unique_ptr<SwitchVirtualDeviceHandler> switchHandler;
        if (virtual_pad_mode == config::VirtualPadMode::MITM)
            switchHandler = std::make_unique<SwitchMITMHandler>(std::move(gamepadPtr), polling_timeout_ms, polling_thread_priority);
        else
            switchHandler = std::make_unique<SwitchHDLHandler>(std::move(gamepadPtr), polling_timeout_ms, polling_thread_priority);

        return InsertHandler(std::move(switchHandler), removable);
    }

    Result Insert(std::unique_ptr<IKeyboard> &&keyboardPtr, bool removable)
    {
        return InsertHandler(std::make_unique<SwitchKeyboardHandler>(std::move(keyboardPtr), virtual_pad_mode, polling_timeout_ms, polling_thread_priority), removable);
    }

    Result Insert(std::unique_ptr<IMouse> &&mousePtr, bool removable)
    {
        return InsertHandler(std::make_unique<SwitchMouseHandler>(std::move(mousePtr), virtual_pad_mode, polling_timeout_ms, polling_thread_priority), removable);
    }

    void RemoveAllNonPlugged(const std::vector<s32> &interfaceIDsPlugged)
    {
        /*
            A handler's destructor joins its polling thread and tears down USB, so it must not
            run while controllerMutex is held. Move the unplugged handlers into this local
            vector under the lock and let it destroy them once the lock is released.
        */
        std::vector<std::unique_ptr<SwitchVirtualDeviceHandler>> unpluggedHandlers;

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

                syscon::logger::LogInfo("Device[%04x-%04x] unplugged !", (*it)->GetDevice()->GetVendor(), (*it)->GetDevice()->GetProduct());

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

    void Exit()
    {
        Clear();
    }
} // namespace syscon::controllers
