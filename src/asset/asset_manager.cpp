#include "asset_manager.h"
#include "../audio/wav_loader.h"
#include "../util/string_util.h"

#include <iostream>

namespace hyperion {

AssetManager *AssetManager::instance = nullptr;

AssetManager *AssetManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new AssetManager();
    }
    return instance;
}

void AssetManager::SetRootDir(const std::string &path) {
    this->root_path = path;
}

const std::string &AssetManager::GetRootDir() const {
    return this->root_path;
}

} // namespace hyperion