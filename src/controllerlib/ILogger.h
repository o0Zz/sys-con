#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdint>

namespace controllerlib
{
    /*
        The one definition of log severity, owned by the library.

        ControllerLib does no logging of its own beyond this interface: a host implements
        ILogger and hands one to each controller, so the library never assumes a sink, a format
        or a platform. Scoping the enum means the int boundary -- hosts typically persist a log
        level as a number in their own config -- has to be crossed explicitly, through
        LogLevelFromInt() below.

        The numeric values are part of this library's public API. Hosts persist them, so do not
        renumber them.
    */
    enum class LogLevel : uint8_t
    {
        Trace = 0,
        Debug = 1,
        Perf = 2,
        Info = 3,
        Warning = 4,
        Error = 5,

        Count
    };

    inline constexpr size_t LogLevelCount = static_cast<size_t>(LogLevel::Count);

    /// Converts a log level as persisted by a host. Out-of-range values clamp to Error, so a
    /// typo quietens the log rather than crashing or logging everything.
    inline constexpr LogLevel LogLevelFromInt(int level)
    {
        if (level < static_cast<int>(LogLevel::Trace))
            return LogLevel::Trace;
        if (level > static_cast<int>(LogLevel::Error))
            return LogLevel::Error;
        return static_cast<LogLevel>(level);
    }

    class ILogger
    {
    public:
        virtual ~ILogger() = default;
        virtual void Log(LogLevel aLogLevel, const char *format, ...) = 0;
        virtual void LogBuffer(LogLevel aLogLevel, const uint8_t *buffer, size_t size) = 0;
        virtual bool IsEnabled(LogLevel aLogLevel) = 0;
    };
} // namespace controllerlib
