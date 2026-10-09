#pragma once

#include "IController.h"
#include "config_handler.h"
#include <switch.h>
namespace syscon::controllers
{
    bool IsAtControllerLimit();

    // removable=false keeps RemoveAllNonPlugged() away from the handler, for a controller that
    // did not come from USB and so has no usbHs interface to be found among the plugged ones.
    // See SwitchDeviceHandler::SetRemovable.
    Result Insert(std::unique_ptr<controllerlib::IController> &&controllerPtr, bool removable = true);
    Result InsertKeyboard(std::unique_ptr<controllerlib::IUSBDevice> &&device);
    Result InsertMouse(std::unique_ptr<controllerlib::IUSBDevice> &&device);
    void RemoveAllNonPlugged(const std::vector<s32> &interfaceIDsPlugged);

    void SetPollingParameters(int32_t _polling_timeout_ms, s8 _thread_priority);

    // Chooses which virtual-pad handler Insert() creates (MITM vs hiddbg/HDLS).
    void SetMode(config::VirtualPadMode mode);

    void Initialize();
    void Clear();
} // namespace syscon::controllers