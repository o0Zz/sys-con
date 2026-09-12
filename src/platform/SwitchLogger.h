#pragma once

/*
    Declares the subset of syscon::logger that ControllerSwitch needs, without dragging in
    logger.h (and with it IFileManager.h). Log levels come from ILogger.h so that there is
    exactly one definition of them: this header used to carry its own LOG_LEVEL_* table that
    had drifted out of step with logger.h's (it was missing PERF, so every level from INFO
    upwards was off by one).
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
    void LogBuffer(LogLevel lvl, const uint8_t *buffer, size_t size);
} // namespace syscon::logger
