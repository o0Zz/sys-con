#pragma once
#include <cstdarg>
#include <string>
#include "IFileManager.h"
#include "ILogger.h"

// Log levels live in ILogger.h -- this header used to carry a duplicate LOG_LEVEL_* macro
// table, and SwitchLogger.h carried a third that had drifted out of step with it.

namespace syscon::logger
{
    void Initialize(const std::string &logPath, std::unique_ptr<IFileManager> &&file);
    void Exit();

    void SetLogLevel(controllerlib::LogLevel level);

    void LogTrace(const char *format, ...);
    void LogDebug(const char *format, ...);
    void LogPerf(const char *format, ...);
    void LogInfo(const char *format, ...);
    void LogWarning(const char *format, ...);
    void LogError(const char *format, ...);

    void Log(controllerlib::LogLevel lvl, const char *fmt, ::std::va_list vl);
    void LogBuffer(controllerlib::LogLevel lvl, const uint8_t *buffer, size_t size);

    class Logger : public controllerlib::ILogger
    {
    public:
        void Log(controllerlib::LogLevel lvl, const char *format, ...) override;
        void LogBuffer(controllerlib::LogLevel lvl, const uint8_t *buffer, size_t size) override;
        bool IsEnabled(controllerlib::LogLevel lvl) override;
    };
} // namespace syscon::logger
