#pragma once
#include "ILogger.h"

class MockLogger : public ILogger
{
public:
    void Log(LogLevel aLogLevel, const char *format, ...) override {}
    void LogBuffer(LogLevel aLogLevel, const uint8_t *buffer, size_t size) override {}
    bool IsEnabled(LogLevel aLogLevel) override { return false; }
};
