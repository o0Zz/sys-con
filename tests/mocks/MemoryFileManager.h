#pragma once

#include "ifilemanager.h"

#include <cstring>
#include <map>
#include <string>

/*
    An in-memory IFileManager, so config parsing can be exercised against exact file
    contents without touching disk. StdFileManager is fine for reading the real shipped
    config.ini, but it can't express the edge cases (a file with no trailing newline, a
    line longer than INI_MAX_LINE) that the reader has to get right.
*/
class MemoryFile final : public syscon::IFile
{
public:
    MemoryFile(std::string *contents, bool append)
        : m_contents(contents), m_offset(append ? contents->size() : 0)
    {
    }

    void close() noexcept override { m_closed = true; }
    bool is_open() const noexcept override { return !m_closed; }

    std::size_t read(void *buffer, std::size_t bytes) noexcept override
    {
        if (m_closed || buffer == nullptr || bytes == 0 || m_offset >= m_contents->size())
            return 0;

        const std::size_t count = std::min(bytes, m_contents->size() - m_offset);
        std::memcpy(buffer, m_contents->data() + m_offset, count);
        m_offset += count;
        return count;
    }

    std::size_t write(const void *buffer, std::size_t bytes) noexcept override
    {
        if (m_closed || buffer == nullptr || bytes == 0)
            return 0;

        m_contents->append(static_cast<const char *>(buffer), bytes);
        m_offset = m_contents->size();
        return bytes;
    }

private:
    std::string *m_contents;
    std::size_t m_offset;
    bool m_closed{false};
};

class MemoryFileManager final : public syscon::IFileManager
{
public:
    MemoryFileManager() = default;

    // Convenience for the common single-file case.
    explicit MemoryFileManager(std::string contents) { m_files["/config.ini"] = std::move(contents); }

    void SetFile(const std::string &path, std::string contents) { m_files[path] = std::move(contents); }
    const std::string &GetFile(const std::string &path) { return m_files[path]; }

    std::unique_ptr<syscon::IFile> open(const std::filesystem::path &path, syscon::OpenFlags flags) override
    {
        auto it = m_files.find(path.string());
        if (it == m_files.end())
        {
            if (!(flags & (syscon::OpenFlags_Write | syscon::OpenFlags_Append)))
                return nullptr; // Reading a file that does not exist.

            it = m_files.emplace(path.string(), std::string()).first;
        }

        return std::make_unique<MemoryFile>(&it->second, (flags & syscon::OpenFlags_Append) != 0);
    }

    bool create_directories(const std::filesystem::path &) override { return true; }

    bool remove(const std::filesystem::path &p) override { return m_files.erase(p.string()) > 0; }

    std::uintmax_t file_size(const std::filesystem::path &p) const override
    {
        auto it = m_files.find(p.string());
        return it == m_files.end() ? 0 : it->second.size();
    }

private:
    std::map<std::string, std::string> m_files;
};
