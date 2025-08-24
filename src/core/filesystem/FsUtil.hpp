/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/utilities/StringUtil.hpp>

#include <string>
#include <cstring>
#include <array>
#include <mutex>

namespace hyperion {

class BufferedReader;

namespace filesystem {

class FilePath;

class FileSystem
{
    static std::mutex s_mtx;
    static Array<FilePath> s_filepaths;

public:
    HYP_DEPRECATED static void PushDirectory(const FilePath& path);
    HYP_DEPRECATED static FilePath PopDirectory();

    static bool DirExists(const std::string& path);
    static int MkDir(const std::string& path);
    static std::string CurrentPath();
    static std::string RelativePath(const std::string& path, const std::string& base);
};
} // namespace filesystem

using filesystem::FileSystem;

} // namespace hyperion
