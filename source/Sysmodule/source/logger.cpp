#include "switch.h"
#include "logger.h"
#include <string.h>
#include <algorithm>
#include <sys/stat.h>
#include <mutex>
#include "SwitchUtils.h"

#define LOG_FILE_SIZE_MAX (128 * 1024)

namespace syscon::logger
{
    static std::mutex printMutex;
    char logBuffer[1024];
    std::string logPath;
    int logLevel = LOG_LEVEL_TRACE;
    char logLevelStr[LOG_LEVEL_COUNT] = {'T', 'D', 'P', 'I', 'W', 'E'};

    Result Initialize(const char *log)
    {
        std::lock_guard<std::mutex> printLock(printMutex);
        s64 fileOffset = 0;

        FsFile file;
        FsFileSystem *fs = fsdevGetDeviceFileSystem("sdmc");

        logPath = std::string(log);

        // Create folder if it doesn't exist.
        std::size_t delimBasePath = logPath.rfind('/');
        if (delimBasePath != std::string::npos)
        {
            std::string basePath = logPath.substr(0, delimBasePath);
            fsFsCreateDirectory(fs, basePath.c_str());
        }

        Result rc = fsFsOpenFile(fs, logPath.c_str(), FsOpenMode_Read, &file);
        if (R_FAILED(rc))
            return rc;

        rc = fsFileGetSize(&file, &fileOffset);
        fsFileClose(&file);
        if (R_FAILED(rc))
            return rc;

        if (fileOffset >= LOG_FILE_SIZE_MAX)
            fsFsDeleteFile(fs, logPath.c_str());

        fsFsCreateFile(fs, logPath.c_str(), 0, 0);
        return 0;
    }

    void SetLogLevel(int level)
    {
        logLevel = level;
    }

    void LogWriteToFile(const char *logBuffer)
    {
        FILE *fp = fopen(logPath.c_str(), "a");
        fwrite(logBuffer, 1, strlen(logBuffer), fp);
        fwrite("\n", 1, 1, fp);
        fclose(fp);
    }

    void Log(int lvl, const char *fmt, ::std::va_list vl)
    {
        if (lvl < logLevel)
            return; // Don't log if the level is lower than the current log level.

        std::lock_guard<std::mutex> printLock(printMutex);

        u64 current_time_ms = SwitchUtils::GetCurrentTimeMs();
        std::snprintf(logBuffer, sizeof(logBuffer), "|%c|%02li:%02li:%02li.%03li|%08X| ", logLevelStr[lvl], (current_time_ms / 3600000) % 24, (current_time_ms / 60000) % 60, (current_time_ms / 1000) % 60, current_time_ms % 1000, (uint32_t)((uint64_t)threadGetSelf()));
        std::vsnprintf(&logBuffer[strlen(logBuffer)], sizeof(logBuffer) - strlen(logBuffer), fmt, vl);

        /* Write in the file. */
        LogWriteToFile(logBuffer);
    }

    void LogBuffer(int lvl, const uint8_t *buffer, size_t size)
    {
        if (lvl < logLevel)
            return; // Don't log if the level is lower than the current log level.

        std::lock_guard<std::mutex> printLock(printMutex);

        u64 current_time_ms = SwitchUtils::GetCurrentTimeMs();
        std::snprintf(logBuffer, sizeof(logBuffer), "|%c|%02li:%02li:%02li.%03li|%08X| ", logLevelStr[lvl], (current_time_ms / 3600000) % 24, (current_time_ms / 60000) % 60, (current_time_ms / 1000) % 60, current_time_ms % 1000, (uint32_t)((uint64_t)threadGetSelf()));

        size_t start_offset = strlen(logBuffer);

        std::snprintf(&logBuffer[strlen(logBuffer)], sizeof(logBuffer) - strlen(logBuffer), "Buffer (%ld): ", size);

        LogWriteToFile(logBuffer);

        /* Format log */
        for (size_t i = 0; i < size; i += 16)
        {
            for (size_t k = 0; k < std::min((size_t)16, size - i); k++)
                snprintf(&logBuffer[start_offset + (k * 3)], sizeof(logBuffer) - (start_offset + (k * 3)), "%02X ", buffer[i + k]);

            /* Write in the file. */
            LogWriteToFile(logBuffer);
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