#include "asset_manager.h"
#include "objloader/obj_loader.h"
#include "ogreloader/ogre_loader.h"
#include "ogreloader/ogre_skeleton_loader.h"
#include "text_loader.h"
#include "texture_loader.h"
#include "../audio/wav_loader.h"
#include "../util/string_util.h"

#include <iostream>

namespace apex {
AssetManager *AssetManager::instance = nullptr;

AssetManager *AssetManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new AssetManager();
    }
    return instance;
}

AssetManager::AssetManager()
{
    // register default loaders
    RegisterLoader<TextLoader>(".txt");
    RegisterLoader<TextLoader>(".inc");
    RegisterLoader<TextLoader>(".glsl");
    RegisterLoader<TextLoader>(".frag");
    RegisterLoader<TextLoader>(".vert");
    RegisterLoader<TextLoader>(".geom");

    RegisterLoader<ObjLoader>(".obj");

    RegisterLoader<OgreLoader>(".mesh.xml");
    RegisterLoader<OgreSkeletonLoader>(".skeleton.xml");

    RegisterLoader<TextureLoader>(".jpg");
    RegisterLoader<TextureLoader>(".png");

    RegisterLoader<WavLoader>(".wav");
}

std::shared_ptr<Loadable> AssetManager::LoadFromFile(const std::string &path, bool use_caching)
{
    if (use_caching) {
        auto it = loaded_assets.find(path);
        if (it != loaded_assets.end()) {
            // reuse already loaded asset
            return it->second;
        }
    }

    try {
        auto &loader = GetLoader(path);
        if (loader != nullptr) {
            auto loaded = loader->LoadFromFile(path);
            if (!loaded) {
                std::cout << "error while loading file: " << path << "\n";
                return nullptr;
            } else {
                loaded->SetFilePath(path);
                if (use_caching) {
                    loaded_assets[path] = loaded;
                }
            }
            return loaded;
        }
    } catch (std::string err) {
        std::cout << "File load error: " << err << "\n";
    }

    return nullptr;
}

const std::unique_ptr<AssetLoader> &AssetManager::GetLoader(const std::string &path)
{
    for (auto &&it : loaders) {
        const std::string &ext = it.first;
        auto &loader = it.second;

        std::string path_lower;
        path_lower.resize(path.length());
        std::transform(path.begin(), path.end(), path_lower.begin(), ::tolower);
        if (StringUtil::EndsWith(path_lower, ext)) {
            return loader;
        }
    }
    throw std::string("No suitable loader found for requested file");
}
} // namespace apex