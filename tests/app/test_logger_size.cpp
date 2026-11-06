#include <gtest/gtest.h>

#include "logger.h"
#include "mocks/MemoryFileManager.h"

#include <string>

TEST(Logger, test_log_file_is_trimmed_while_running)
{
    constexpr const char *kLogPath = "/log.txt";
    constexpr size_t kRuntimeMax = 1024 * 1024;

    MemoryFileManager fileManager("");
    syscon::logger::Initialize(kLogPath, fileManager);
    syscon::logger::SetLogLevel(controllerlib::LogLevel::Trace);

    const std::string payload(200, 'x');
    for (int i = 0; i < 8000; i++)
        syscon::logger::LogInfo("%s", payload.c_str());

    const std::uintmax_t size = fileManager.file_size(kLogPath);
    syscon::logger::Exit();

    EXPECT_GT(size, 0u);
    EXPECT_LE(size, kRuntimeMax);
}
