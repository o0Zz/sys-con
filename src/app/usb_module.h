#pragma once

#include "config_handler.h"
#include "IFileManager.h"
namespace syscon::usb
{
    int Initialize(IFileManager &fileManager, const syscon::config::GlobalConfig &config);
    void Exit();
} // namespace syscon::usb
