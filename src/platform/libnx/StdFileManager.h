#pragma once
#include "IFileManager.h"
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>

// The host tests drive this implementation too (tests/app/test_config_store.cpp reads the
// shipped config.ini through it), and MSVC spells POSIX mkdir differently and without a mode.
#ifdef _WIN32
    #include <direct.h>
    #define syscon_mkdir(path) ::_mkdir(path)
#else
    #define syscon_mkdir(path) ::mkdir(path, 0777)
#endif

namespace syscon
{
    class StdFile final : public IFile
    {
    public:
        explicit StdFile(std::FILE *file)
            : m_file(file)
        {
        }

        ~StdFile() override
        {
            close();
        }

        void close() noexcept override
        {
            if (m_file != nullptr)
            {
                std::fclose(m_file);
                m_file = nullptr;
            }
        }

        bool is_open() const noexcept override
        {
            return m_file != nullptr;
        }

        std::size_t read(void *buffer, std::size_t bytes) noexcept override
        {
            if (m_file == nullptr || !buffer || bytes == 0)
                return 0;

            return std::fread(buffer, 1, bytes, m_file);
        }

        std::size_t write(const void *buffer, std::size_t bytes) noexcept override
        {
            if (m_file == nullptr || !buffer || bytes == 0)
                return 0;

            return std::fwrite(buffer, 1, bytes, m_file);
        }

    private:
        std::FILE *m_file;
    };

    class StdFileManager final : public IFileManager
    {
    public:
        ~StdFileManager() override = default;

        std::unique_ptr<IFile> open(const std::string &path, OpenFlags flags) override
        {
            const char *mode = (flags & OpenFlags_Append) ? "ab" : ((flags & OpenFlags_Write) ? "wb" : "rb");

            std::FILE *file = std::fopen(path.c_str(), mode);
            if (file == nullptr)
                return nullptr;

            return std::make_unique<StdFile>(file);
        }

        bool create_directories(const std::string &dir) override
        {
            for (std::size_t slash = dir.find('/', 1); slash != std::string::npos; slash = dir.find('/', slash + 1))
                syscon_mkdir(dir.substr(0, slash).c_str());

            return syscon_mkdir(dir.c_str()) == 0 || errno == EEXIST;
        }

        bool remove(const std::string &p) override
        {
            return std::remove(p.c_str()) == 0;
        }

        std::uintmax_t file_size(const std::string &p) const override
        {
            struct stat info;
            if (::stat(p.c_str(), &info) != 0)
                return 0;

            return static_cast<std::uintmax_t>(info.st_size);
        }
    };
} // namespace syscon
