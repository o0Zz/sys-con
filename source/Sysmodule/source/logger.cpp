#include "logger.h"
#include <string.h>
#include <algorithm>
#include <sys/stat.h>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stratosphere.hpp>
#include <stratosphere/fs/fs_filesystem.hpp>
#include <stratosphere/fs/fs_file.hpp>

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

        const char klogLevelStr[LOG_LEVEL_COUNT] = {'T', 'D', 'P', 'I', 'W', 'E'};
    } // namespace
#ifdef ATMOSPHERE_OS_HORIZON

    void Initialize(const std::string &log)
    {
        std::scoped_lock printLock(sLogMutex);
        s64 fileOffset = 0;
        ams::fs::FileHandle file;

        sLogPath = std::filesystem::path(log);
        std::filesystem::path basePath = sLogPath.parent_path();
        ams::fs::CreateDirectory(basePath.c_str());

        // Check if the log file is too big, if so, delete it.
        if (R_SUCCEEDED(ams::fs::OpenFile(std::addressof(file), sLogPath.c_str(), ams::fs::OpenMode_Read)))
        {
            ams::fs::GetFileSize(&fileOffset, file);
            ams::fs::CloseFile(file);
        }
        if (fileOffset >= LOG_FILE_SIZE_MAX)
            ams::fs::DeleteFile(sLogPath.c_str());

        // Create the log file if it doesn't exist (Or previously deleted)
        ams::fs::CreateFile(sLogPath.c_str(), 0);
    }

    void LogWriteToFile(const char *logBuffer)
    {
        s64 fileOffset;
        ams::fs::FileHandle file;

        ams::fs::OpenFile(std::addressof(file), sLogPath.c_str(), ams::fs::OpenMode_Write | ams::fs::OpenMode_AllowAppend);
        ON_SCOPE_EXIT { ams::fs::CloseFile(file); };

        ams::fs::GetFileSize(&fileOffset, file);

        ams::fs::WriteFile(file, fileOffset, logBuffer, strlen(logBuffer), ams::fs::WriteOption::Flush);
        ams::fs::WriteFile(file, fileOffset + strlen(logBuffer), "\n", 1, ams::fs::WriteOption::Flush);
    }
#else
    void Initialize(const std::string &log)
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
    }

    void LogWriteToFile(const char *logBuffer)
    {
        std::ofstream logFile(sLogPath, std::ios::app);
        if (!logFile.is_open())
            return;

        logFile << logBuffer << "\n";
    }
#endif

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
        std::snprintf(sLogBuffer, sizeof(sLogBuffer), "|%c|%02lu:%02lu:%02lu.%03lu|%08X| ", klogLevelStr[lvl], (current_time_ms / 3600000) % 24, (current_time_ms / 60000) % 60, (current_time_ms / 1000) % 60, current_time_ms % 1000, (uint32_t)std::hash<std::thread::id>{}(std::this_thread::get_id()));
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
        std::snprintf(sLogBuffer, sizeof(sLogBuffer), "|%c|%02lu:%02lu:%02lu.%03lu|%08X| ", klogLevelStr[lvl], (current_time_ms / 3600000) % 24, (current_time_ms / 60000) % 60, (current_time_ms / 1000) % 60, current_time_ms % 1000, (uint32_t)std::hash<std::thread::id>{}(std::this_thread::get_id()));

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

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
void LogAMS(const char *fmt, ...)
{
    static const char *SYSLOG_IP = "192.168.10.245";
    static const int SYSLOG_PORT = 514; // default syslog UDP port

    int sockfd;
    struct sockaddr_in servaddr;

    std::va_list vl;
    va_start(vl, fmt);

    std::lock_guard<std::mutex> printLock(syscon::logger::sLogMutex);
    syscon::logger::sLogBuffer[0] = 'A';
    syscon::logger::sLogBuffer[1] = 'M';
    syscon::logger::sLogBuffer[2] = 'S';
    syscon::logger::sLogBuffer[3] = '|';
    syscon::logger::sLogBuffer[4] = '\0'; // Initialize the buffer
                                          // Format timestamp and prefix
                                          /*uint64_t current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                                          std::snprintf(syscon::logger::sLogBuffer, sizeof(syscon::logger::sLogBuffer), "|%c|%02lu:%02lu:%02lu.%03lu| ", 'W', (current_time_ms / 3600000) % 24, (current_time_ms / 60000) % 60, (current_time_ms / 1000) % 60, current_time_ms % 1000);
                                          std::vsnprintf(&syscon::logger::sLogBuffer[strlen(syscon::logger::sLogBuffer)], sizeof(syscon::logger::sLogBuffer) - strlen(syscon::logger::sLogBuffer), fmt, vl);
                                          */
    va_end(vl);

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        return;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SYSLOG_PORT);

    if (inet_pton(AF_INET, SYSLOG_IP, &servaddr.sin_addr) <= 0)
    {
        perror("invalid syslog server IP");
        close(sockfd);
        return;
    }

    // Syslog message format: <PRI>MESSAGE
    // PRI  = Facility*8 + Severity. Example: local0 facility (16) and info (6) = 134
    char syslog_msg[2048];
    int pri = (16 * 8) + 6; // local0.info
    std::snprintf(syslog_msg, sizeof(syslog_msg), "<%d>%s", pri, syscon::logger::sLogBuffer);

    sendto(sockfd,
           syslog_msg,
           strlen(syslog_msg),
           0,
           (const struct sockaddr *)&servaddr,
           sizeof(servaddr));

    close(sockfd);
}
