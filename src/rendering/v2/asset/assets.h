#ifndef HYPERION_V2_ASSETS_H
#define HYPERION_V2_ASSETS_H

#include "model_loaders/obj_model_loader.h"
#include "../components/node.h"

#include <util/static_map.h>

#include <string>
#include <unordered_map>

namespace hyperion::v2 {

class Engine;

template <class ...T>
constexpr bool resolution_failure = false;

class AssetCacheStore {
public:
    AssetCacheStore();
    AssetCacheStore(const AssetCacheStore &other) = delete;
    AssetCacheStore &operator=(const AssetCacheStore &other) = delete;
    ~AssetCacheStore();

    void GC();
};

class Assets {
    struct FunctorBase {
        std::string filepath;

        FunctorBase() = delete;
        FunctorBase(const std::string &filepath) : filepath(filepath) {}

        LoaderFormat GetResourceFormat() const
        {
            constexpr StaticMap<const char *, LoaderFormat, 1> extensions{
                std::make_pair("obj", LoaderFormat::MODEL_OBJ)
            };

            std::string path_lower(filepath);
            std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), std::tolower);

            for (const auto &it : extensions.pairs) {
                if (StringUtil::EndsWith(path_lower, it.first)) {
                    return it.second;
                }
            }

            return LoaderFormat::NONE;
        }

        template <class T, LoaderFormat Format>
        auto GetLoader() const -> typename LoaderObject<T, Format>::Loader
        {
            return typename LoaderObject<T, Format>::Loader();
        }
        
        template <class Loader>
        auto LoadResource(Engine *engine, const Loader &loader)
        {
            auto results = loader.Instance()
                .Enqueue(filepath, {
                    .stream = std::make_unique<LoaderStream>(filepath)
                })
                .Load();

            /* TODO: deal with cache here */
            
            return loader.Build(engine, results.front().second);
        }

        template <class Loader, class ...Args>
        auto LoadResources(Engine *engine, const Loader &loader, Args &&... args)
        {
            auto instance = loader.Instance();

            const std::array<std::string, sizeof...(args)> filepaths{args...};

            for (const auto &filepath : filepaths) {
                instance.Enqueue(filepath, {
                    .stream = std::make_unique<LoaderStream>(filepath)
                });
            }

            auto results = instance.Load();

            /* TODO: deal with cache here */

            std::array<std::unique_ptr<typename Loader::FinalType>, sizeof...(args)> constructed_objects;

            for (size_t i = 0; i < results.size(); i++) {
                if (!results[i].first) {
                    continue;
                }

                constructed_objects[i] = loader.Build(engine, results[i].second);
            }

            return std::move(constructed_objects);
        }
    };

    template <class T>
    struct Functor : FunctorBase {
        std::unique_ptr<T> operator()(Engine *engine)
        {
            static_assert(resolution_failure<T>, "No handler defined for the given type");

            return nullptr;
        }
    };

    template <>
    struct Functor<Node> : FunctorBase {
        std::unique_ptr<Node> operator()(Engine *engine)
        {
            switch (GetResourceFormat()) {
            case LoaderFormat::MODEL_OBJ:
                return LoadResource(engine, GetLoader<Node, LoaderFormat::MODEL_OBJ>());
            default:
                return nullptr;
            }
        }
    };

    template <class Type, class ...Args>
    auto LoadMultiple(Args &&... paths)
    {
        const std::array<std::string, sizeof...(paths)> filepaths{paths...};
        std::array<std::unique_ptr<Type>, sizeof...(paths)> results{};

        for (size_t i = 0; i < filepaths.size(); i++) {
            const std::string &filepath = filepaths[i];

            results[i] = std::move(Functor<Type>(filepath)(m_engine));
        }

        return std::move(results);
    }

public:
    Assets(Engine *engine);
    Assets(const Assets &other) = delete;
    Assets &operator=(const Assets &other) = delete;
    ~Assets();

    template <class Type>
    auto Load(const std::string &filepath) -> std::unique_ptr<Type>
    {
        return std::move(LoadMultiple<Type>(filepath).front());
    }

    template <class Type, class ...Args>
    auto Load(const std::string &filepath, Args &&... other_paths)
    {
        return std::move(LoadMultiple<Type>(filepath, std::move(other_paths)...));
    }

private:
    Engine *m_engine;
    
};

} // namespace hyperion::v2

#endif