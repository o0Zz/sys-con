#pragma once
#include "ILogger.h"

class MockLogger : public controllerlib::ILogger
{
public:
    void Log(controllerlib::LogLevel aLogLevel, const char *format, ...) override {}
    void LogBuffer(controllerlib::LogLevel aLogLevel, const uint8_t *buffer, size_t size) override {}
    bool IsEnabled(controllerlib::LogLevel aLogLevel) override { return false; }
};
