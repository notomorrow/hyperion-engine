#ifndef HYPERION_V2_ASSET_LOADER_HPP
#define HYPERION_V2_ASSET_LOADER_HPP

#include <core/Containers.hpp>
#include <core/Handle.hpp>
#include <scene/Node.hpp>

#include "Loader.hpp"
#include "LoaderObject.hpp"

namespace hyperion::v2 {

class AssetManager;

struct LoadedAsset
{
    LoaderResult result;
    UniquePtr<void> ptr;
};

class AssetLoaderBase
{
public:
    virtual ~AssetLoaderBase() = default;

    virtual LoadedAsset Load(AssetManager &asset_manager, const String &path) const = 0;
};

template <class T>
struct AssetLoaderWrapper
{
    using ResultType = HandleBase;
    using CastedType = Handle<T>;

    AssetLoaderBase &loader;

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    static inline ResultType EmptyResult()
        { return ResultType(); }

    template <class Engine>
    HandleBase Load(AssetManager &asset_manager, Engine *engine, const String &path)
    {
        auto loader_result = loader.Load(asset_manager, path);

        return engine->template CreateHandle<T>(loader_result.ptr.template Cast<T>().Release());
    }
};

template <>
struct AssetLoaderWrapper<Node>
{
    using ResultType = NodeProxy;
    using CastedType = NodeProxy;

    AssetLoaderBase &loader;

    AssetLoaderWrapper(AssetLoaderBase &loader)
        : loader(loader)
    {
    }

    static inline ResultType EmptyResult()
        { return ResultType(); }

    template <class Engine>
    NodeProxy Load(AssetManager &asset_manager, Engine *, const String &path)
    {
        auto loader_result = loader.Load(asset_manager, path);

        return NodeProxy(loader_result.ptr.Cast<Node>().Release());
    }
};

} // namespace hyperion::v2

#endif