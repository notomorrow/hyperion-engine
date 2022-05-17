#include "fs_util.h"
#include <util/defines.h>

#include <sys/stat.h>

#if HYP_WINDOWS
#include <direct.h>
#endif

namespace hyperion::v2 {

bool FileSystem::DirExists(const std::string &path)
{
    struct stat st;
    
    if (stat(path.c_str(), &st) == 0) {
        return st.st_mode & S_IFDIR;
    }

    return false;
}

int FileSystem::Mkdir(const std::string &path)
{
#if HYP_WINDOWS
    return ::_mkdir(path.c_str());
#elif _POSIX_C_SOURCE
    return ::mkdir(path.c_str());
#else
    return ::mkdir(path.c_str(), 0755);
#endif
}

} // namespace hyperion::v2