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
            { return typename LoaderObject<T, Format>::Loader(); }
        
        template <class Loader>
        auto LoadResource(Engine *engine, const Loader &loader)
        {
            auto results = loader.Instance()
                .Enqueue(filepath, {
                    .stream = std::make_unique<LoaderStream>(filepath)
                })
                .Load();

            /* TODO: deal with cache here */

            DebugLog(LogType::Info, "Constructing loaded asset %s...\n", filepath.c_str());

            return loader.Build(engine, results.front().second);
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
    auto LoadAsync(Args &&... paths)
    {
        const std::array<std::string,     sizeof...(paths)> filepaths{paths...};
        std::array<std::unique_ptr<Type>, sizeof...(paths)> results{};

        std::vector<std::thread> threads;
        threads.reserve(results.size());
        
        for (size_t i = 0; i < filepaths.size(); i++) {
            threads.emplace_back([index = i, engine = m_engine, &filepaths, &results] {
                DebugLog(LogType::Info, "Loading asset %s...\n", filepaths[index].c_str());
                results[index] = Functor<Type>(filepaths[index])(engine);
            });
        }

        for (auto &thread : threads) {
            thread.join();
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
        return std::move(LoadAsync<Type>(filepath).front());
    }

    template <class Type, class ...Args>
    auto Load(const std::string &filepath, Args &&... other_paths)
    {
        return std::move(LoadAsync<Type>(filepath, std::move(other_paths)...));
    }

private:
    Engine *m_engine;
    
};

} // namespace hyperion::v2

#endif