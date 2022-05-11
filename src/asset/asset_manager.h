#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <memory>
#include <deque>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <mutex>

namespace hyperion {
class AssetManager {
public:
    static AssetManager *GetInstance();

    void SetRootDir(const std::string &path);
    const std::string &GetRootDir() const;

private:
    static AssetManager *instance;

    std::string root_path = "./";
};
}

#endif
