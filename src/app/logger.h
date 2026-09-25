#pragma once
#include <cstdarg>
#include <cstdio>
#include <string>
#include "IFileManager.h"
#include "ILogger.h"

// Log levels live in ILogger.h.

/* newlib keeps two printf engines: referencing vsnprintf/snprintf links the float-capable
   one, which drags in dtoa and the wide-char conversions for ~5 KiB. No format string in the
   sysmodule uses a float conversion, so the device build takes the integer-only variants --
   and a %f added anywhere below would print garbage rather than a number. */
#ifdef __SWITCH__
#define SYSCON_VSNPRINTF vsniprintf
#define SYSCON_SNPRINTF  sniprintf
#else
#define SYSCON_VSNPRINTF std::vsnprintf
#define SYSCON_SNPRINTF  std::snprintf
#endif

namespace syscon::logger
{
    void Initialize(const std::string &logPath, IFileManager &file);
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
