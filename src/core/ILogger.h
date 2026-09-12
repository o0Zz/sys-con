#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdint>

/*
    The one definition of log severity.

    There used to be three: this enum, a LOG_LEVEL_* macro table in logger.h, and a second
    macro table in SwitchLogger.h whose values had drifted out of step (it was missing PERF,
    so everything from INFO upwards was off by one). Scoping it means the remaining int
    boundary -- config.ini stores log_level as a number -- has to be crossed explicitly,
    through LogLevelFromInt() below, instead of implicitly anywhere.

    The numeric values are part of the user-facing config format; see the log_level comment
    in dist/config/sys-con/config.ini. Do not renumber them.
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

/// Converts a log_level as written in config.ini. Out-of-range values clamp to Error, so a
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