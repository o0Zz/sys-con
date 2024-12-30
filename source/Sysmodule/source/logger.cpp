#include "logger.h"
#include <string.h>
#include <algorithm>
#include <sys/stat.h>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <iostream>

#define LOG_FILE_SIZE_MAX (128 * 1024)

namespace syscon::logger
{
    static std::mutex sLogMutex;

    static char sLogBuffer[1024];
    static std::filesystem::path sLogPath;
    static int slogLevel = LOG_LEVEL_TRACE;

    const char klogLevelStr[LOG_LEVEL_COUNT] = {'T', 'D', 'P', 'I', 'W', 'E'};

    Result Initialize(const std::string &log)
    {
        std::lock_guard<std::mutex> printLock(sLogMutex);

        // Extract the directory from the log path
        sLogPath = std::filesystem::path(log);
        std::filesystem::path basePath = sLogPath.parent_path();

        // Create the directory if it doesn't exist
        if (!basePath.empty() && !std::filesystem::exists(basePath))
            std::filesystem::create_directories(basePath);

        // Check the file size if it exists
        if (std::filesystem::exists(sLogPath))
        {
            auto fileSize = std::filesystem::file_size(sLogPath);
            if (fileSize >= LOG_FILE_SIZE_MAX)
                std::filesystem::remove(sLogPath);
        }

        return 0;
    }

    void SetLogLevel(int level)
    {
        slogLevel = level;
    }

    void LogWriteToFile(const char *logBuffer)
    {
        std::ofstream logFile(sLogPath, std::ios::app);
        if (!logFile.is_open())
            return;

        logFile << logBuffer << "\n";
    }

    void Log(int lvl, const char *fmt, ::std::va_list vl)
    {
        if (lvl < slogLevel)
            return; // Don't log if the level is lower than the current log level.

        std::lock_guard<std::mutex> printLock(sLogMutex);

        u64 current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::snprintf(sLogBuffer, sizeof(sLogBuffer), "|%c|%02li:%02li:%02li.%03li|%08X| ", klogLevelStr[lvl], (current_time_ms / 3600000) % 24, (current_time_ms / 60000) % 60, (current_time_ms / 1000) % 60, current_time_ms % 1000, (uint32_t)((uint64_t)threadGetSelf()));
        std::vsnprintf(&sLogBuffer[strlen(sLogBuffer)], sizeof(sLogBuffer) - strlen(sLogBuffer), fmt, vl);

        /* Write in the file. */
        LogWriteToFile(sLogBuffer);
    }

    void LogBuffer(int lvl, const uint8_t *buffer, size_t size)
    {
        if (lvl < slogLevel)
            return; // Don't log if the level is lower than the current log level.

        std::lock_guard<std::mutex> printLock(sLogMutex);

        u64 current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::snprintf(sLogBuffer, sizeof(sLogBuffer), "|%c|%02li:%02li:%02li.%03li|%08X| ", klogLevelStr[lvl], (current_time_ms / 3600000) % 24, (current_time_ms / 60000) % 60, (current_time_ms / 1000) % 60, current_time_ms % 1000, (uint32_t)((uint64_t)threadGetSelf()));

        size_t start_offset = strlen(sLogBuffer);

        std::snprintf(&sLogBuffer[strlen(sLogBuffer)], sizeof(sLogBuffer) - strlen(sLogBuffer), "Buffer (%ld): ", size);

        LogWriteToFile(sLogBuffer);

        /* Format log */
        for (size_t i = 0; i < size; i += 16)
        {
            for (size_t k = 0; k < std::min((size_t)16, size - i); k++)
                snprintf(&sLogBuffer[start_offset + (k * 3)], sizeof(sLogBuffer) - (start_offset + (k * 3)), "%02X ", buffer[i + k]);

            /* Write in the file. */
            LogWriteToFile(sLogBuffer);
        }
    }

    void LogTrace(const char *fmt, ...)
    {
        ::std::va_list vl;
        va_start(vl, fmt);
        Log(LOG_LEVEL_TRACE, fmt, vl);
        va_end(vl);
    }

    void LogDebug(const char *fmt, ...)
    {
        ::std::va_list vl;
        va_start(vl, fmt);
        Log(LOG_LEVEL_DEBUG, fmt, vl);
        va_end(vl);
    }

    void LogInfo(const char *fmt, ...)
    {
        ::std::va_list vl;
        va_start(vl, fmt);
        Log(LOG_LEVEL_INFO, fmt, vl);
        va_end(vl);
    }

    void LogWarning(const char *fmt, ...)
    {
        ::std::va_list vl;
        va_start(vl, fmt);
        Log(LOG_LEVEL_WARNING, fmt, vl);
        va_end(vl);
    }

    void LogError(const char *fmt, ...)
    {
        ::std::va_list vl;
        va_start(vl, fmt);
        Log(LOG_LEVEL_ERROR, fmt, vl);
        va_end(vl);
    }

    void Exit()
    {
    }

    void Logger::Log(LogLevel lvl, const char *format, ::std::va_list vl)
    {
        syscon::logger::Log(lvl, format, vl);
    }

    void Logger::LogBuffer(LogLevel lvl, const uint8_t *buffer, size_t size)
    {
        syscon::logger::LogBuffer(lvl, buffer, size);
    }

} // namespace syscon::logger