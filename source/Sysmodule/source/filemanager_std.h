#pragma once
#include "ifilemanager.h"
#include <fstream>
namespace syscon
{
    class StdFile final : public IFile
    {
    public:
        StdFile(std::fstream &&fs, const std::filesystem::path &path)
            : m_file(std::move(fs)), m_path(path)
        {
        }

        ~StdFile() override
        {
            if (m_file.is_open())
                m_file.close();
        }

        bool is_open() const noexcept override
        {
            return m_file.is_open();
        }

        std::size_t read(void *buffer, std::size_t bytes) noexcept override
        {
            if (!m_file.is_open() || !buffer || bytes == 0)
                return 0;

            m_file.read(static_cast<char *>(buffer), static_cast<std::streamsize>(bytes));
            return static_cast<std::size_t>(m_file.gcount());
        }

        std::size_t write(const void *buffer, std::size_t bytes) noexcept override
        {
            if (!m_file.is_open() || !buffer || bytes == 0)
                return 0;

            m_file.write(static_cast<const char *>(buffer), static_cast<std::streamsize>(bytes));
            if (m_file.bad())
                return 0;

            return bytes;
        }

    private:
        std::fstream m_file;
        std::filesystem::path m_path;
    };

    class StdFileManager final : public IFileManager
    {
    public:
        ~StdFileManager() override = default;

        std::unique_ptr<IFile> open(const std::filesystem::path &path, OpenFlags flags) override
        {
            std::ios_base::openmode mode = std::ios::binary;
            if (flags & OpenFlags_Read)
                mode |= std::ios::in;
            if (flags & OpenFlags_Write)
                mode |= std::ios::out;
            if (flags & OpenFlags_Append)
                mode |= std::ios::app;

            std::fstream logFile(path, mode);
            if (!logFile.is_open())
                return nullptr;

            return std::make_unique<StdFile>(std::move(logFile), path);
        }

        bool create_directories(const std::filesystem::path &dir) override
        {
            if (!dir.empty() && !std::filesystem::exists(dir))
                std::filesystem::create_directories(dir);

            return true;
        }

        bool remove(const std::filesystem::path &p) override
        {
            return std::filesystem::remove(p);
        }

        std::uintmax_t file_size(const std::filesystem::path &p) const override
        {
            return std::filesystem::file_size(p);
        }
    };
} // namespace syscon