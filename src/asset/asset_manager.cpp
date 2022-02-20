#include "asset_manager.h"
#include "objloader/obj_loader.h"
#include "objloader/mtl_loader.h"
#include "ogreloader/ogre_loader.h"
#include "ogreloader/ogre_skeleton_loader.h"
#include "text_loader.h"
#include "texture_loader.h"
#include "../audio/wav_loader.h"
#include "../util/string_util.h"

#include <iostream>

namespace hyperion {
const AssetLoader::Result AssetManager::ErrorList::no_error{ AssetLoader::Result::ASSET_OK };
const int AssetManager::ErrorList::max_size = 16;

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
    RegisterLoader<TextLoader>(".comp");
    RegisterLoader<TextLoader>(".spv");

    RegisterLoader<ObjLoader>(".obj");
    RegisterLoader<MtlLoader>(".mtl");

    RegisterLoader<OgreLoader>(".mesh.xml");
    RegisterLoader<OgreSkeletonLoader>(".skeleton.xml");

    RegisterLoader<TextureLoader>(".jpg");
    RegisterLoader<TextureLoader>(".png");
    RegisterLoader<TextureLoader>(".tga");
    RegisterLoader<TextureLoader>(".psd");
    RegisterLoader<TextureLoader>(".bmp");
    RegisterLoader<TextureLoader>(".gif");
    RegisterLoader<TextureLoader>(".hdr");

    RegisterLoader<WavLoader>(".wav");
}

void AssetManager::SetRootDir(const std::string &path) {
    this->root_path = path;
}

const std::string &AssetManager::GetRootDir() const {
    return this->root_path;
}

std::shared_ptr<Loadable> AssetManager::LoadFromFile(const std::string &path, bool use_caching)
{
    const std::string trimmed_path = StringUtil::Trim(StringUtil::ReplaceAll(path, "\\", "/"));

    // paths to try a few times before giving up, in order
    const std::array<std::string, 2> try_paths = {
         this->GetRootDir() + trimmed_path,
         trimmed_path
    };

    if (use_caching) {
        //std::lock_guard guard(load_asset_mtx);

        for (const auto &path : try_paths) {
            auto it = loaded_assets.find(path);
            if (it != loaded_assets.end() && it->second != nullptr) {
                // reuse already loaded asset
                const auto clone = it->second->Clone();

                if (clone == nullptr) { // no implementation; return shared ptr
                    std::cout << "Use cached object (direct) " << path << "\n";

                    return it->second;
                }

                std::cout << "Use cached object (clone) " << path << "\n";

                return clone;
            }
        }
    }

    auto &loader = GetLoader(new_path);

    if (loader != nullptr) {
        for (const auto &p : try_paths) {
            auto result = loader->LoadFromFile(p);

            if (!result) {
                m_error_list.Add(result);

                DebugLog(LogType::Warn, "Asset could not be loaded at %s:  %s\n", p.c_str(), result.message.c_str());
            } else if (result.loadable == nullptr) {
                DebugLog(LogType::Warn, "Asset loader gave ASSET_OK result but asset was null at %s:  %s\n", p.c_str(), result.message.c_str());
            } else {
                result.loadable->SetFilePath(p);

                loaded_assets[p] = result.loadable;

                return result.loadable;
            }
        }
    }

    return nullptr;
}

const std::unique_ptr<AssetLoader> &AssetManager::GetLoader(const std::string &path)
{
    for (auto &&it : loaders) {
        const std::string &ext = it.first;
        auto &loader = it.second;

        std::string path_lower(path);
        std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), ::tolower);
        if (StringUtil::EndsWith(path_lower, ext)) {
            return loader;
        }
    }

    throw (std::string("No suitable loader found for requested file: ") + path);
}
} // namespace hyperion