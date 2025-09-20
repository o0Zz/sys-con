#pragma once
#include "ifilemanager.h"
#include <stratosphere.hpp>
#include <stratosphere/fs/fs_filesystem.hpp>
#include <stratosphere/fs/fs_file.hpp>

namespace syscon
{
    class AMSFile : public IFile
    {
    public:
        AMSFile(ams::fs::FileHandle &&file, int flags, const std::filesystem::path &&path)
            : m_file(std::move(file)), m_path(std::move(path)), m_fileoffset(0)
        {
            if ((flags & ams::fs::OpenMode_AllowAppend))
                ams::fs::GetFileSize(&m_fileoffset, m_file);
        }

        ~AMSFile() override
        {
            close();
        }

        void close() noexcept override
        {
            if (m_file.handle != INVALID_HANDLE)
            {
                ams::fs::CloseFile(m_file);
                m_file.handle = INVALID_HANDLE;
            }
        }

        bool is_open() const noexcept override
        {
            return m_file.handle != INVALID_HANDLE;
        }

        std::size_t read(void *buffer, std::size_t bytes) noexcept override
        {
            if (!m_file.handle != INVALID_HANDLE || !buffer || bytes == 0)
                return 0;

            ams::Result err = ams::fs::ReadFile(m_file, m_fileoffset, buffer, bytes);
            if (R_FAILED(err))
                return 0;

            m_fileoffset += bytes;
            return static_cast<std::size_t>(bytes);
        }

        std::size_t write(const void *buffer, std::size_t bytes) noexcept override
        {
            if (!m_file.handle != INVALID_HANDLE || !buffer || bytes == 0)
                return 0;

            ams::Result err = ams::fs::WriteFile(m_file, m_fileoffset, buffer, bytes, ams::fs::WriteOption::Flush);
            if (R_FAILED(err))
                return 0;

            m_fileoffset += bytes;
            return static_cast<std::size_t>(bytes);
        }

    private:
        ams::fs::FileHandle m_file;
        std::filesystem::path m_path;
        s64 m_fileoffset;
    };

    class AMSFileManager final : public IFileManager
    {
    private:
        static std::string to_ams_path(const std::filesystem::path &path) { return "sdmc:/" + path.string(); }

    public:
        ~AMSFileManager() override = default;

        std::unique_ptr<IFile> open(const std::filesystem::path &path, OpenFlags flags) override
        {
            int mode = 0;
            if (flags & OpenFlags_Read)
                mode |= ams::fs::OpenMode_Read;
            if (flags & OpenFlags_Write)
                mode |= ams::fs::OpenMode_Write;
            if (flags & OpenFlags_Append)
                mode |= ams::fs::OpenMode_AllowAppend;

            if ((mode & ams::fs::OpenMode_Write) || (mode & ams::fs::OpenMode_AllowAppend))
                ams::fs::CreateFile(to_ams_path(path).c_str(), 0);

            ams::fs::FileHandle file;
            if (R_FAILED(ams::fs::OpenFile(std::addressof(file), to_ams_path(path).c_str(), mode)))
                return nullptr;

            return std::make_unique<AMSFile>(std::move(file), (::syscon::OpenFlags)mode, std::move(path));
        }

        bool create_directories(const std::filesystem::path &dir) override
        {
            if (R_FAILED(ams::fs::CreateDirectory(to_ams_path(dir).c_str())))
                return false;
            return true;
        }

        bool remove(const std::filesystem::path &path) override
        {
            if (R_FAILED(ams::fs::DeleteFile(to_ams_path(path).c_str())))
                return false;
            return true;
        }

        std::uintmax_t file_size(const std::filesystem::path &path) const override
        {
            ams::fs::FileHandle file;
            s64 fileOffset = 0;

            if (R_SUCCEEDED(ams::fs::OpenFile(std::addressof(file), to_ams_path(path).c_str(), ams::fs::OpenMode_Read)))
            {
                ams::fs::GetFileSize(&fileOffset, file);
                ams::fs::CloseFile(file);
            }

            return fileOffset;
        }
    };

} // namespace syscon