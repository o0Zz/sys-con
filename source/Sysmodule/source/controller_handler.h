#pragma once

#include "IController.h"
#include <switch.h>
namespace syscon::controllers
{
    bool IsAtControllerLimit();

    Result Insert(std::unique_ptr<IController> &&controllerPtr);
    void RemoveAllNonPlugged(std::vector<s32> interfaceIDsPlugged);

    void SetPollingParameters(int32_t _polling_timeout_ms, s8 _thread_priority);

    void Initialize();
    void Clear();
    void Exit();
} // namespace syscon::controllers