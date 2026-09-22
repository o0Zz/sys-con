/*
    Edge cases for the buffered line reader that feeds inih.

    The reader used to issue one IFile::read() per byte and got two boundary cases wrong:
    a final line with no trailing newline was dropped, and a line longer than INI_MAX_LINE
    made it report EOF, silently discarding the rest of the file. Both are exercised here
    through the real config parser rather than the reader directly, since the reader lives
    in an anonymous namespace.
*/
#include <gtest/gtest.h>

#include "config_handler.h"
#include "ini.h"
#include "mocks/MemoryFileManager.h"

#include <string>

namespace
{

    constexpr const char *kConfigPath = "/config.ini";

    syscon::config::GlobalConfig LoadFrom(const std::string &contents)
    {
        MemoryFileManager fileManager(contents);
        ::syscon::config::Initialize(fileManager);

        syscon::config::GlobalConfig config;
        ::syscon::config::LoadGlobalConfig(kConfigPath, &config);
        return config;
    }

} // namespace

TEST(IniReader, test_reads_ordinary_file)
{
    syscon::config::GlobalConfig config = LoadFrom("[global]\npolling_timeout_ms=42\n");

    EXPECT_EQ(config.polling_timeout_ms, 42);
}

TEST(IniReader, test_last_line_without_trailing_newline_is_not_dropped)
{
    // No '\n' after the final key. The old reader consumed it and then returned nullptr,
    // so the value never reached the handler.
    syscon::config::GlobalConfig config = LoadFrom("[global]\npolling_timeout_ms=42");

    EXPECT_EQ(config.polling_timeout_ms, 42);
}

TEST(IniReader, test_overlong_line_does_not_discard_rest_of_file)
{
    /*
        A comment line comfortably longer than INI_MAX_LINE. The old reader filled its
        buffer, returned nullptr, and inih read that as end-of-file, so every subsequent
        line was ignored. Splitting the line across two reads (as fgets does) keeps both
        halves comments and lets parsing continue.
    */
    const std::string overlong(INI_MAX_LINE * 2, ';');
    syscon::config::GlobalConfig config = LoadFrom("[global]\n" + overlong + "\npolling_timeout_ms=42\n");

    EXPECT_EQ(config.polling_timeout_ms, 42);
}

TEST(IniReader, test_blank_and_crlf_lines_are_tolerated)
{
    syscon::config::GlobalConfig config = LoadFrom("[global]\r\n\r\npolling_timeout_ms=42\r\n\r\n");

    EXPECT_EQ(config.polling_timeout_ms, 42);
}

TEST(IniReader, test_empty_file_yields_defaults)
{
    syscon::config::GlobalConfig defaults;
    syscon::config::GlobalConfig config = LoadFrom("");

    EXPECT_EQ(config.polling_timeout_ms, defaults.polling_timeout_ms);
}
