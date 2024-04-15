/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <core/lib/filesystem/FilePath.hpp>
#include <util/fs/FsUtil.hpp>

#include <asset/BufferedByteReader.hpp>

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

int FilePath::MkDir() const
{
#if HYP_WINDOWS
    return ::_mkdir(Data());
#else
    return ::mkdir(Data(), 0755);
#endif
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
    
    if (stat(Data(), &st) == 0) {
        return st.st_mode & S_IFDIR;
    }

    return false;
}

uint64 FilePath::LastModifiedTimestamp() const
{
    struct stat st;
    
    if (stat(Data(), &st) != 0) {
        return 0;
    }

#ifdef HYP_MACOS
    return st.st_mtimespec.tv_sec;
#else
    return st.st_mtime;
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

bool FilePath::Open(BufferedReader &out) const
{
    if (!Exists()) {
        return false;
    }

    out = BufferedReader(*this);

    return true;
}

Array<FilePath> FilePath::GetAllFilesInDirectory() const
{
    Array<FilePath> files;

    for (const auto &entry : std::filesystem::directory_iterator(Data())) {
        if (entry.is_regular_file()) {
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

    for (const auto &entry : std::filesystem::directory_iterator(Data())) {
        if (entry.is_regular_file()) {
            size += entry.file_size();
        }
    }

    return size;
}

SizeType FilePath::FileSize() const
{
    struct stat st;

    if (stat(Data(), &st) != 0) {
        return 0;
    }

    return st.st_size;
}

} // namespace hyperion