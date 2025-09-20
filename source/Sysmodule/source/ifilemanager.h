#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace syscon
{
    enum OpenFlags
    {
        OpenFlags_None = 0,
        OpenFlags_Read = (1 << 0),
        OpenFlags_Write = (1 << 1),
        OpenFlags_Append = (1 << 2),
    };

    class IFile
    {
    public:
        virtual ~IFile() = default;

        virtual void close() noexcept = 0;
        virtual bool is_open() const noexcept = 0;

        virtual std::size_t read(void *buffer, std::size_t bytes) noexcept = 0;
        virtual std::size_t write(const void *buffer, std::size_t bytes) noexcept = 0;
    };

    class IFileManager
    {
    public:
        virtual ~IFileManager() = default;

        virtual std::unique_ptr<IFile> open(const std::filesystem::path &path, OpenFlags flags) = 0;

        virtual bool create_directories(const std::filesystem::path &dir) = 0;
        virtual bool remove(const std::filesystem::path &p) = 0;

        virtual std::uintmax_t file_size(const std::filesystem::path &p) const = 0;
    };

} // namespace syscon