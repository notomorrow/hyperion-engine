#ifndef HYPERION_V2_ASSETS_H
#define HYPERION_V2_ASSETS_H

#include "model_loaders/obj_model_loader.h"
#include "../components/node.h"

#include <string>
#include <unordered_map>

namespace hyperion::v2 {

class AssetCacheStore {
public:
    AssetCacheStore();
    AssetCacheStore(const AssetCacheStore &other) = delete;
    AssetCacheStore &operator=(const AssetCacheStore &other) = delete;
    ~AssetCacheStore();

    void GC();
};

class Assets {
public:
    Assets();
    Assets(const Assets &other) = delete;
    Assets &operator=(const Assets &other) = delete;
    ~Assets();
    
    auto LoadModel(const std::string &filepath)
    {
        switch (GetResourceFormat(filepath)) {
        case LoaderFormat::MODEL_OBJ:
            return std::move(LoadResources<Node, LoaderFormat::MODEL_OBJ>(filepath).front());
        default:
            return std::unique_ptr<Node>(nullptr);
        }
    }

private:
    LoaderFormat GetResourceFormat(const std::string &filepath) const
    {
        std::string path_lower(filepath);
        std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), std::tolower);

        const std::array<std::pair<std::string, LoaderFormat>, 1> extension_mappings{
            std::make_pair("obj", LoaderFormat::MODEL_OBJ)
        };

        for (const auto &it : extension_mappings) {
            if (StringUtil::EndsWith(path_lower, it.first)) {
                return it.second;
            }
        }

        return LoaderFormat::NONE;
    }

    template <LoaderFormat Format>
    auto GetLoader() const
    {
        if constexpr (Format == LoaderFormat::MODEL_OBJ) {
            return ObjModelLoader();
        } else {
            return Loader<void>({
                [](...) {
                    return LoaderResult{LoaderResult::Status::ERR, "No suitable loader found"};
                }
            });
        }
    }

    template <class FinalType, LoaderFormat Format, class ...Args>
    auto LoadResources(Args &&... args)
    {
        const std::array<std::string, sizeof...(args)> filepaths{
            args...
        };

        auto loader = GetLoader<Format>();
        auto instance = loader.Instance();

        for (const auto &filepath : filepaths) {
            instance.Enqueue(filepath, {
                .stream = std::make_unique<LoaderStream>(filepath)
            });
        }

        auto results = instance.Load();

        /* TODO: deal with cache here */

        std::array<std::unique_ptr<FinalType>, sizeof...(args)> constructed_objects;

        for (size_t i = 0; i < results.size(); i++) {
            if (!results[i].first) {
                continue;
            }

            constructed_objects[i] = loader.Build(results[i].second);
        }

        return std::move(constructed_objects);
    }
};

} // namespace hyperion::v2

#endif