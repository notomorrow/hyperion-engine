#ifndef HYPERION_V2_ASSETS_H
#define HYPERION_V2_ASSETS_H

#include "model_loaders/obj_model_loader.h"
#include "material_loaders/mtl_material_loader.h"
#include "model_loaders/ogre_xml_model_loader.h"
#include "skeleton_loaders/ogre_xml_skeleton_loader.h"
#include "texture_loaders/texture_loader.h"
#include "../components/node.h"

#include <util/static_map.h>

#include <string>
#include <unordered_map>
#include <thread>
#include <algorithm>

namespace hyperion::v2 {

class Engine;

template <class ...T>
constexpr bool resolution_failure = false;

class Assets {
    struct HandleAssetFunctorBase {
        std::string filepath;

        LoaderFormat GetResourceFormat() const
        {
            constexpr StaticMap<const char *, LoaderFormat, 12> extensions{
                std::make_pair(".obj",          LoaderFormat::OBJ_MODEL),
                std::make_pair(".mtl",          LoaderFormat::MTL_MATERIAL_LIBRARY),
                std::make_pair(".mesh.xml",     LoaderFormat::OGRE_XML_MODEL),
                std::make_pair(".skeleton.xml", LoaderFormat::OGRE_XML_SKELETON),
                std::make_pair(".png",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".jpg",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".jpeg",         LoaderFormat::TEXTURE_2D),
                std::make_pair(".tga",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".bmp",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".psd",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".gif",          LoaderFormat::TEXTURE_2D),
                std::make_pair(".hdr",          LoaderFormat::TEXTURE_2D)
            };

            std::string path_lower(filepath);
            std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), [](char ch){ return std::tolower(ch); });

            for (const auto &it : extensions.pairs) {
                if (StringUtil::EndsWith(path_lower, it.first)) {
                    return it.second;
                }
            }

            return LoaderFormat::NONE;
        }

        template <class T, LoaderFormat Format>
        auto GetLoader() const -> typename LoaderObject<T, Format>::Loader
            { return typename LoaderObject<T, Format>::Loader(); }
        
        template <class Loader>
        auto LoadResource(Engine *engine, const Loader &loader) -> std::unique_ptr<typename Loader::FinalType>
        {
            auto result = loader.Instance().Load(LoaderState{
                .filepath = filepath,
                .stream = {filepath},
                .engine = engine
            });

            if (result.first) {
                /* TODO: deal with cache here */

                DebugLog(LogType::Info, "Constructing loaded asset %s...\n", filepath.c_str());

                return loader.Build(engine, result.second);
            } else {
                DebugLog(LogType::Error, "Failed to load asset %s: %s\n", filepath.c_str(), result.first.message.c_str());

                return nullptr;
            }
        }
    };

    template <class T>
    struct HandleAssetFunctor : HandleAssetFunctorBase {
        std::unique_ptr<T> operator()(Engine *engine)
        {
            static_assert(resolution_failure<T>, "No handler defined for the given type");

            return nullptr;
        }
    };

    template <>
    struct HandleAssetFunctor<Node> : HandleAssetFunctorBase {
        std::unique_ptr<Node> operator()(Engine *engine)
        {
            switch (GetResourceFormat()) {
            case LoaderFormat::OBJ_MODEL:
                return LoadResource(engine, GetLoader<Node, LoaderFormat::OBJ_MODEL>());
            case LoaderFormat::OGRE_XML_MODEL:
                return LoadResource(engine, GetLoader<Node, LoaderFormat::OGRE_XML_MODEL>());
            default:
                return nullptr;
            }
        }
    };

    template <>
    struct HandleAssetFunctor<Skeleton> : HandleAssetFunctorBase {
        std::unique_ptr<Skeleton> operator()(Engine *engine)
        {
            switch (GetResourceFormat()) {
            case LoaderFormat::OGRE_XML_SKELETON:
                return LoadResource(engine, GetLoader<Skeleton, LoaderFormat::OGRE_XML_SKELETON>());
            default:
                return nullptr;
            }
        }
    };

    template <>
    struct HandleAssetFunctor<Texture> : HandleAssetFunctorBase {
        std::unique_ptr<Texture> operator()(Engine *engine)
        {
            return LoadResource(engine, GetLoader<Texture, LoaderFormat::TEXTURE_2D>());
        }
    };

    template <>
    struct HandleAssetFunctor<MaterialGroup> : HandleAssetFunctorBase {
        std::unique_ptr<MaterialGroup> operator()(Engine *engine)
        {
            return LoadResource(engine, GetLoader<MaterialGroup, LoaderFormat::MTL_MATERIAL_LIBRARY>());
        }
    };

    template <class Type, class ...Args>
    auto LoadAsync(Args &&... paths)
    {
        const std::array<std::string,     sizeof...(paths)> filepaths{paths...};
        std::array<std::unique_ptr<Type>, sizeof...(paths)> results{};

        std::vector<std::thread> threads;
        threads.reserve(results.size());
        
        for (size_t i = 0; i < filepaths.size(); i++) {
            threads.emplace_back([index = i, engine = m_engine, &filepaths, &results] {
                DebugLog(LogType::Info, "Loading asset %s...\n", filepaths[index].c_str());

                auto functor = HandleAssetFunctor<Type>{};
                functor.filepath = filepaths[index];

                if (!(results[index] = functor(engine))) {
                    DebugLog(LogType::Warn, "%s: The asset could not be loaded and will be returned as null.\n\t"
                        "Any usages or indirection may result in the application crashing!\n", functor.filepath.c_str());
                }
            });
        }

        for (auto &thread : threads) {
            thread.join();
        }

        return std::move(results);
    }

    template <class Type>
    auto LoadAsyncVector(std::vector<std::string> filepaths) -> std::vector<std::unique_ptr<Type>>
    {
        std::vector<std::unique_ptr<Type>> results{};
        results.resize(filepaths.size());

        std::vector<std::thread> threads;
        threads.reserve(results.size());
        
        for (size_t i = 0; i < filepaths.size(); i++) {
            threads.emplace_back([index = i, engine = m_engine, &filepaths, &results] {
                DebugLog(LogType::Info, "Loading asset %s...\n", filepaths[index].c_str());

                auto functor = HandleAssetFunctor<Type>{};
                functor.filepath = filepaths[index];

                results[index] = functor(engine);
            });
        }

        for (auto &thread : threads) {
            thread.join();
        }

        return results;
    }

public:
    Assets(Engine *engine);
    Assets(const Assets &other) = delete;
    Assets &operator=(const Assets &other) = delete;
    ~Assets();

    /*! \brief Load a single asset from the given path. If no asset could be loaded, nullptr is returned.
     * @param filepath The path of the asset
     * @returns A unique pointer to Type
     */
    template <class Type>
    auto Load(const std::string &filepath) -> std::unique_ptr<Type>
    {
        return std::move(LoadAsync<Type>(filepath).front());
    }

    /*! \brief Loads a collection of assets asynchronously from one another (the function returns when all have
     *   been loaded). For any assets that could not be loaded, nullptr will be in the array in place of the
     *   asset.
     * @param filepath The path of the first asset to be loaded
     * @param other_paths Argument pack of assets 1..n to be loaded
     * @returns An array of unique pointers to Type
     */
    template <class Type, class ...Args>
    auto Load(const std::string &filepath, Args &&... other_paths)
    {
        return std::move(LoadAsync<Type>(filepath, std::move(other_paths)...));
    }

    /*! \brief Loads a collection of assets asynchronously from one another (the function returns when all have
     *   been loaded). For any assets that could not be loaded, nullptr will be in the vector in place of the
     *   asset.
     * @param filepaths A vector of filepaths of files to load
     * @returns A vector of unique pointers to Type
     */
    template <class Type>
    auto Load(const std::vector<std::string> &filepaths)
    {
        return LoadAsyncVector<Type>(filepaths);
    }

private:
    Engine *m_engine;
    
};

} // namespace hyperion::v2

#endif