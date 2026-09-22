#pragma once

/*
    Declares the subset of syscon::logger that ControllerSwitch needs, without dragging in
    logger.h (and with it IFileManager.h). Log levels come from ILogger.h so that there is
    exactly one definition of them.
*/
#include "ILogger.h"

namespace syscon::logger
{
    void LogTrace(const char *fmt, ...);
    void LogDebug(const char *fmt, ...);
    void LogPerf(const char *fmt, ...);
    void LogInfo(const char *fmt, ...);
    void LogWarning(const char *fmt, ...);
    void LogError(const char *fmt, ...);
    void LogBuffer(controllerlib::LogLevel lvl, const uint8_t *buffer, size_t size);
} // namespace syscon::logger
