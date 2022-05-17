#ifndef HYPERION_V2_FS_UTIL_H
#define HYPERION_V2_FS_UTIL_H

#include <string>

namespace hyperion::v2 {

class FileSystem {
public:
    static bool DirExists(const std::string &path);
    static int Mkdir(const std::string &path);
};

} // namespace hyperion::v2

#endif