/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <core/filesystem/FilePath.hpp>
#include <core/filesystem/FsUtil.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <filesystem>
#include <fstream>

#include <sys/stat.h>

#if defined(HYP_WINDOWS)
#include <direct.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#elif defined(HYP_UNIX)
#include <unistd.h>
#endif

namespace hyperion {
namespace filesystem {

bool FilePath::MkDir() const
{
    return Exists() || std::filesystem::create_directories(Data());
}

bool FilePath::CanWrite() const
{
    struct stat st;

    if (stat(Data(), &st) != 0)
    {
        return false;
    }

    return (st.st_mode & S_IWUSR) != 0 || (st.st_mode & S_IWGRP) != 0 || (st.st_mode & S_IWOTH) != 0;
}

bool FilePath::CanRead() const
{
    struct stat st;
    if (stat(Data(), &st) != 0)
    {
        return false;
    }

    return (st.st_mode & S_IRUSR) != 0 || (st.st_mode & S_IRGRP) != 0 || (st.st_mode & S_IROTH) != 0;
}

HYP_API String FilePath::GetExtension() const
{
    return StringUtil::GetExtension(*this);
}

HYP_NODISCARD String FilePath::StripExtension() const
{
    return StringUtil::StripExtension(*this);
}

bool FilePath::Remove() const
{
    return std::filesystem::remove(Data());
}

bool FilePath::Exists() const
{
    struct stat st;

    return stat(Data(), &st) == 0;
}

bool FilePath::IsDirectory() const
{
    struct stat st;

    if (stat(Data(), &st) == 0)
    {
        return st.st_mode & S_IFDIR;
    }

    return false;
}

Time FilePath::LastModifiedTimestamp() const
{
    struct stat st;

    if (stat(Data(), &st) != 0)
    {
        return Time(0);
    }

#ifdef HYP_MACOS
    return Time(st.st_mtimespec.tv_sec);
#else
    return Time(st.st_mtime);
#endif
}

String FilePath::Basename() const
{
    return String(StringUtil::Basename(Data()).c_str());
}

FilePath FilePath::BasePath() const
{
    return FilePath(StringUtil::BasePath(Data()).c_str());
}

hyperion::Array<FilePath> FilePath::GetAllFilesInDirectory() const
{
    hyperion::Array<FilePath> files;

    for (const auto& entry : std::filesystem::directory_iterator(Data()))
    {
        if (entry.is_regular_file())
        {
#ifdef HYP_WINDOWS
            files.PushBack(WideString(entry.path().c_str()).ToUTF8());
#else
            files.PushBack(entry.path().c_str());
#endif
        }
    }

    return files;
}

hyperion::Array<FilePath> FilePath::GetSubdirectories() const
{
    hyperion::Array<FilePath> files;

    for (const auto& entry : std::filesystem::directory_iterator(Data()))
    {
        if (entry.is_directory())
        {
#ifdef HYP_WINDOWS
            files.PushBack(WideString(entry.path().c_str()).ToUTF8());
#else
            files.PushBack(entry.path().c_str());
#endif
        }
    }

    return files;
}

SizeType FilePath::DirectorySize() const
{
    SizeType size = 0;

    for (const auto& entry : std::filesystem::directory_iterator(Data()))
    {
        if (entry.is_regular_file())
        {
            size += entry.file_size();
        }
    }

    return size;
}

SizeType FilePath::FileSize() const
{
    struct stat st;

    if (stat(Data(), &st) != 0)
    {
        return 0;
    }

    return st.st_size;
}

} // namespace filesystem
} // namespace hyperion