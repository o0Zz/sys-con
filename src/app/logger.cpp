#include "logger.h"
#include <string.h>
#include <algorithm>
#include <sys/stat.h>
#include <mutex>
#include <thread>
#include <inttypes.h>

#define LOG_FILE_SIZE_MAX (128 * 1024)

using namespace controllerlib;

namespace syscon::logger
{
    namespace
    {
        // Mutex to protect log writing
        static std::mutex sLogMutex;

        static std::string sLogPath;
        static LogLevel sLogLevel = LogLevel::Trace;
        static IFileManager *sFileManager = nullptr;

        constexpr size_t LogLineMax = 512;

        const char kLogLevelStr[LogLevelCount] = {'T', 'D', 'P', 'I', 'W', 'E'};

        size_t FormatHeader(char *line, size_t lineSize, LogLevel lvl)
        {
            uint64_t current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            int written = SYSCON_SNPRINTF(line, lineSize, "|%c|%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ".%03" PRIu64 "|%08X| ",
                                          kLogLevelStr[static_cast<size_t>(lvl)],
                                          (current_time_ms / 3600000) % 24,
                                          (current_time_ms / 60000) % 60,
                                          (current_time_ms / 1000) % 60,
                                          current_time_ms % 1000,
                                          (uint32_t)std::hash<std::thread::id>{}(std::this_thread::get_id()));

            return written < 0 ? 0 : std::min((size_t)written, lineSize - 1);
        }

        size_t AppendFormatted(char *line, size_t lineSize, size_t offset, const char *fmt, ::std::va_list vl)
        {
            if (offset >= lineSize)
                return offset;

            const size_t space = lineSize - offset;
            int written = SYSCON_VSNPRINTF(&line[offset], space, fmt, vl);
            if (written < 0)
                return offset;

            /* vsnprintf returns the length it would have written, not what it wrote: clamping to
               the former would append uninitialised stack bytes to the line. */
            return offset + std::min((size_t)written, space - 1);
        }
    } // namespace

    void Initialize(const std::string &log, IFileManager &file)
    {
        sLogPath = log;
        sFileManager = &file;

        std::lock_guard<std::mutex> printLock(sLogMutex);

        sFileManager->create_directories(sLogPath.substr(0, sLogPath.find_last_of('/')));

        if (sFileManager->file_size(sLogPath) >= LOG_FILE_SIZE_MAX)
            sFileManager->remove(sLogPath);
    }

    void Exit()
    {
        std::lock_guard<std::mutex> printLock(sLogMutex);
        sFileManager = nullptr;
    }

    /* The file is reopened per line on purpose: holding a write handle on log.txt for the process
       lifetime blocks every other reader of it -- the console-side tooling that streams the log,
       and anything pulling it off the SD card while sys-con runs. */
    void LogWriteToFile(const char *logBuffer, size_t length)
    {
        std::lock_guard<std::mutex> printLock(sLogMutex);

        if (sFileManager == nullptr)
            return;

        auto file = sFileManager->open(sLogPath, (OpenFlags)(OpenFlags_Write | OpenFlags_Append));
        if (file && file->is_open())
            file->write(logBuffer, length);
    }

    void SetLogLevel(LogLevel level)
    {
        // This function is not thread safe, should be called only once at the start of the program
        sLogLevel = level;
    }

    void Log(LogLevel lvl, const char *fmt, ::std::va_list vl)
    {
        if (lvl < sLogLevel)
            return; // Don't log if the level is lower than the current log level.

        char line[LogLineMax];

        size_t length = FormatHeader(line, sizeof(line) - 1, lvl);
        length = AppendFormatted(line, sizeof(line) - 1, length, fmt, vl);
        line[length++] = '\n';

        LogWriteToFile(line, length);
    }

    void LogBuffer(LogLevel lvl, const uint8_t *buffer, size_t size)
    {
        if (lvl < sLogLevel)
            return; // Don't log if the level is lower than the current log level.

        char line[LogLineMax];

        size_t start_offset = FormatHeader(line, sizeof(line) - 1, lvl);

        const size_t space = sizeof(line) - 1 - start_offset;
        int written = SYSCON_SNPRINTF(&line[start_offset], space, "Buffer (%zu): \n", size);
        if (written > 0)
            LogWriteToFile(line, start_offset + std::min((size_t)written, space - 1));

        for (size_t i = 0; i < size; i += 16)
        {
            size_t length = start_offset;
            for (size_t k = 0; k < std::min((size_t)16, size - i); k++)
                length += SYSCON_SNPRINTF(&line[length], sizeof(line) - 1 - length, "%02X ", buffer[i + k]);

            line[length++] = '\n';
            LogWriteToFile(line, length);
        }
    }

#define DEFINE_LOG_FUNCTION(name, level) \
    void name(const char *fmt, ...)      \
    {                                    \
        ::std::va_list vl;               \
        va_start(vl, fmt);               \
        Log(LogLevel::level, fmt, vl);   \
        va_end(vl);                      \
    }

    DEFINE_LOG_FUNCTION(LogTrace, Trace)
    DEFINE_LOG_FUNCTION(LogDebug, Debug)
    DEFINE_LOG_FUNCTION(LogPerf, Perf)
    DEFINE_LOG_FUNCTION(LogInfo, Info)
    DEFINE_LOG_FUNCTION(LogWarning, Warning)
    DEFINE_LOG_FUNCTION(LogError, Error)

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
        return lvl >= sLogLevel;
    }

} // namespace syscon::logger
