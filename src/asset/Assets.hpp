#ifndef HYPERION_V2_ASSETS_H
#define HYPERION_V2_ASSETS_H

#include <asset/AssetBatch.hpp>
#include <asset/AssetLoader.hpp>
#include <scene/Node.hpp>

#include <core/Containers.hpp>
#include <util/fs/FsUtil.hpp>
#include <util/Defines.hpp>
#include <Threads.hpp>
#include <Constants.hpp>
#include <Component.hpp>

#include <TaskSystem.hpp>

#include <string>
#include <unordered_map>
#include <thread>
#include <algorithm>
#include <type_traits>

namespace hyperion::v2 {

class Engine;

using LoadAssetResultPair = Pair<LoaderResult, UniquePtr<void>>;

class AssetManager
{
    friend struct AssetBatch;
    friend class AssetLoader;

public:
    AssetManager(Engine *engine)
        : m_engine(engine)
    {
    }

    AssetManager(const AssetManager &other) = delete;
    AssetManager &operator=(const AssetManager &other) = delete;
    ~AssetManager() = default;

    const FilePath &GetBasePath() const
        { return m_base_path; }

    void SetBasePath(const FilePath &base_path)
        { m_base_path = base_path; }

    Engine *GetEngine() const
        { return m_engine; }

    template <class Loader, class ... Formats>
    void Register(Formats &&... formats)
    {
        static_assert(std::is_base_of_v<AssetLoaderBase, Loader>,
            "Loader must be a derived class of AssetLoaderBase!");

        // Threads::AssertOnThread(THREAD_GAME);

        const FixedArray<String, sizeof...(formats)> format_strings {
            formats...
        };

        for (auto &str : format_strings) {
            m_loaders[str] = UniquePtr<Loader>::Construct();
        }
    }

    /*! \brief Load a single asset in a synchronous fashion. The resulting object will have a
        type corresponding to the provided template type.
        Node -> NodeProxy
        T -> Handle<T> */
    template <class T>
    auto Load(const String &path) -> typename AssetLoaderWrapper<NormalizedType<T>>::CastedType
    {
        const String extension(StringUtil::GetExtension(path.Data()).c_str());

        if (extension.Empty()) {
            return AssetLoaderWrapper<NormalizedType<T>>::EmptyResult();
        }

        AssetLoaderBase *loader = nullptr;

        { // find loader for the requested type
            const auto it = m_loaders.Find(extension);

            if (it != m_loaders.End()) {
                loader = it->second.Get();
            } else {
                for (auto &it : m_loaders) {
                    if (path.EndsWith(it.first)) {
                        loader = it.second.Get();
                        break;
                    }
                }
            }
        }

        AssertThrowMsg(loader != nullptr,
            "No loader for type! Path: [%s]", extension.Data());

        return AssetLoaderWrapper<NormalizedType<T>>(*loader)
            .Load(*this, m_engine, path);
    }

    template <class T>
    auto LoadForBatch(const String &path) -> typename AssetLoaderWrapper<NormalizedType<T>>::ResultType
    {
        return Load<T>(path);
    }

    /*! \brief Load multple objects of the same type. All assets will be loaded asynchronously
        from one another, but the method is still synchronous, so there is no need to wait on anything
        after calling this method. All assets will be returned in the resulting array in the order that their
        respective filepaths were provided in. */
    template <class T, class ... Paths>
    auto Load(const String &first_path, Paths &&... paths) -> FixedArray<typename AssetLoaderWrapper<NormalizedType<T>>::CastedType, sizeof...(paths) + 1>
    {
        FixedArray<typename AssetLoaderWrapper<NormalizedType<T>>::CastedType, sizeof...(paths) + 1> results_array;
        FixedArray<String, sizeof...(paths) + 1> paths_array { first_path, paths... };
        AssetBatch batch = CreateBatch();

        for (const auto &path : paths_array) {
            batch.Add<T>(path, path);
        }

        batch.LoadAsync();
        auto results = batch.AwaitResults();

        AssertThrow(results.Size() == results_array.Size());

        SizeType index = 0u;

        for (auto &it : results) {
            results_array[index++] = it.second.Get<T>();
        }

        return results_array;
    }

    AssetBatch CreateBatch()
    {
        return AssetBatch(*this);
    }

private:
    ComponentSystem &GetObjectSystem();

    Engine *m_engine;
    FilePath m_base_path;
    FlatMap<String, UniquePtr<AssetLoaderBase>> m_loaders;
};

class AssetLoader : public AssetLoaderBase
{
protected:
    static inline auto GetTryFilepaths(const FilePath &filepath, const FilePath &original_filepath)
    {
        const auto current_path = FilePath::Current();

        FixedArray<FilePath, 3> paths {
            FilePath::Relative(original_filepath, current_path),
            FilePath::Relative(filepath, current_path),
            filepath
        };

        return paths;
    }

public:
    virtual ~AssetLoader() = default;

    virtual LoadedAsset Load(AssetManager &asset_manager, const String &path) const override final
    {
        LoadedAsset asset;
        asset.result = LoaderResult {
            LoaderResult::Status::ERR_NOT_FOUND,
            "File could not be found"
        };

        const auto original_filepath = FilePath(path);
        const auto filepath = GetRebasedFilepath(asset_manager, original_filepath);
        const auto paths = GetTryFilepaths(filepath, original_filepath);

        for (const auto &path : paths) {
            LoaderState state {
                .filepath = path.Data(),
                .stream = path.Open(),
                .asset_manager = &asset_manager
            };

            if (!state.stream.IsOpen()) {
                // could not open... try next path
                DebugLog(
                    LogType::Warn,
                    "Could not open file at path : %s, trying next path...\n",
                    path.Data()
                );

                continue;
            }

            auto [result, ptr] = LoadAsset(state);
            asset.result = result;

            if (result) {
                asset.ptr = std::move(ptr);

                return asset;
            }
        }

        return asset;
    }

protected:
    virtual LoadAssetResultPair LoadAsset(LoaderState &state) const = 0;

    FilePath GetRebasedFilepath(AssetManager &asset_manager, String filepath) const
    {
        filepath = FilePath::Relative(filepath, FilePath::Current());

        if (asset_manager.GetBasePath().Any()) {
            return FilePath::Join(
                asset_manager.GetBasePath().Data(),
                filepath.Data()
            );
        }

        return filepath;
    }
};

} // namespace hyperion::v2

#endif
