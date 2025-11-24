#include "logger.h"
#include <string.h>
#include <algorithm>
#include <sys/stat.h>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <inttypes.h>

#define LOG_FILE_SIZE_MAX (128 * 1024)

namespace syscon::logger
{
    namespace
    {
        // Mutex to protect log writing
        static std::mutex sLogMutex;

        static char sLogBuffer[1024];
        static std::filesystem::path sLogPath;
        static int slogLevel = LOG_LEVEL_TRACE;
        static std::unique_ptr<IFileManager> sFileManager;

        const char klogLevelStr[LOG_LEVEL_COUNT] = {'T', 'D', 'P', 'I', 'W', 'E'};
    } // namespace

    void Initialize(const std::string &log, std::unique_ptr<IFileManager> &&file)
    {
        sLogPath = std::filesystem::path(log);
        sFileManager = std::move(file);

        std::lock_guard<std::mutex> printLock(sLogMutex);
        std::filesystem::path basePath = sLogPath.parent_path();

        sFileManager->create_directories(basePath);

        if (sFileManager->file_size(sLogPath) >= LOG_FILE_SIZE_MAX)
            sFileManager->remove(sLogPath);
    }

    void Exit()
    {
        std::lock_guard<std::mutex> printLock(sLogMutex);
        sFileManager.reset();
    }

    void LogWriteToFile(const char *logBuffer)
    {
        if (sFileManager == nullptr)
            return;

        auto file = sFileManager->open(sLogPath, (OpenFlags)(OpenFlags_Write | OpenFlags_Append));
        if (file && file->is_open())
        {
            file->write(logBuffer, strlen(logBuffer));
            file->write("\n", 1);
        }
    }

    void SetLogLevel(int level)
    {
        // This function is not thread safe, should be called only once at the start of the program
        slogLevel = level;
    }

    void Log(int lvl, const char *fmt, ::std::va_list vl)
    {
        if (lvl < slogLevel)
            return; // Don't log if the level is lower than the current log level.

        std::lock_guard<std::mutex> printLock(sLogMutex);

        uint64_t current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::snprintf(sLogBuffer, sizeof(sLogBuffer), "|%c|%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ".%03" PRIu64 "|%08X| ", klogLevelStr[lvl], (current_time_ms / 3600000) % 24, (current_time_ms / 60000) % 60, (current_time_ms / 1000) % 60, current_time_ms % 1000, (uint32_t)std::hash<std::thread::id>{}(std::this_thread::get_id()));
        std::vsnprintf(&sLogBuffer[strlen(sLogBuffer)], sizeof(sLogBuffer) - strlen(sLogBuffer), fmt, vl);

        /* Write in the file. */
        LogWriteToFile(sLogBuffer);
    }

    void LogBuffer(int lvl, const uint8_t *buffer, size_t size)
    {
        if (lvl < slogLevel)
            return; // Don't log if the level is lower than the current log level.

        std::lock_guard<std::mutex> printLock(sLogMutex);

        uint64_t current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::snprintf(sLogBuffer, sizeof(sLogBuffer), "|%c|%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ".%03" PRIu64 "|%08X| ", klogLevelStr[lvl], (current_time_ms / 3600000) % 24, (current_time_ms / 60000) % 60, (current_time_ms / 1000) % 60, current_time_ms % 1000, (uint32_t)std::hash<std::thread::id>{}(std::this_thread::get_id()));

        size_t start_offset = strlen(sLogBuffer);

        std::snprintf(&sLogBuffer[strlen(sLogBuffer)], sizeof(sLogBuffer) - strlen(sLogBuffer), "Buffer (%zu): ", size);

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

    void LogPerf(const char *fmt, ...)
    {
        ::std::va_list vl;
        va_start(vl, fmt);
        Log(LOG_LEVEL_PERF, fmt, vl);
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

    void Logger::Log(LogLevel lvl, const char *format, ...)
    {
        ::std::va_list vl;
        va_start(vl, format);
        syscon::logger::Log(lvl, format, vl);
        va_end(vl);
    }

    void Logger::LogBuffer(LogLevel lvl, const uint8_t *buffer, size_t size)
    {
        syscon::logger::LogBuffer(lvl, buffer, size);
    }

    bool Logger::IsEnabled(LogLevel lvl)
    {
        return lvl >= slogLevel;
    }

} // namespace syscon::logger
