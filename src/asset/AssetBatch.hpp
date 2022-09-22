#ifndef HYPERION_V2_ASSET_BATCH_HPP
#define HYPERION_V2_ASSET_BATCH_HPP

#include <asset/AssetLoader.hpp>
#include <core/Containers.hpp>
#include <core/Handle.hpp>
#include <scene/Node.hpp>
#include <TaskSystem.hpp>

#include <type_traits>

namespace hyperion::v2 {

class AssetManager;

struct EnqueuedAsset
{
    String path;
    Variant<NodeProxy, HandleBase> value;

    /*! \brief Retrieve the value of the loaded asset. If the requested type does not match
        the type of the value, if any, held within the variant, no error should occur and an 
        empty container will be returned. */
    template <class T>
    auto Get() -> typename AssetLoaderWrapper<T>::CastedType
    {
        using Wrapper = AssetLoaderWrapper<T>;

        if constexpr (std::is_same_v<typename Wrapper::CastedType, typename Wrapper::ResultType>) {
            if (value.Is<typename Wrapper::ResultType>()) {
                return value.Get<typename Wrapper::ResultType>();
            }
        } else {
            if (value.Is<typename Wrapper::ResultType>()) {
                return value.Get<typename Wrapper::ResultType>().template Cast<T>();
            }
        }

        return typename Wrapper::CastedType();
    }

    explicit operator bool() const { return value.IsValid(); }
};

struct AssetBatch : private TaskBatch
{
private:
    template <class T>
    struct LoadObjectWrapper
    {
        template <class AssetManager, class AssetBatch>
        HYP_FORCE_INLINE void operator()(
            AssetManager &asset_manager,
            AssetBatch &batch,
            SizeType index
        )
        {
            AssertThrow(index < batch.enqueued_assets.Size());

            batch.enqueued_assets[index].value
                = static_cast<typename AssetLoaderWrapper<T>::ResultType>(asset_manager.template Load<T>(batch.enqueued_assets[index].path));
        }
    };

public:
    using TaskBatch::IsCompleted;

    DynArray<EnqueuedAsset> enqueued_assets;

    AssetBatch(AssetManager &asset_manager)
        : TaskBatch(),
          asset_manager(asset_manager)
    {
    }

    AssetBatch(const AssetBatch &other) = delete;
    AssetBatch &operator=(const AssetBatch &other) = delete;
    AssetBatch(AssetBatch &&other) noexcept = delete;
    AssetBatch &operator=(AssetBatch &&other) noexcept = delete;
    ~AssetBatch() = default;

    /*! \brief Enqueue an asset of type T to be loaded in this batch.
        Only call this method before LoadAsync() is called. */
    template <class T>
    void Add(const String &path)
    {
        AssertThrowMsg(IsCompleted(),
            "Cannot add assets while loading!");

        const auto asset_index = enqueued_assets.Size();

        enqueued_assets.PushBack(EnqueuedAsset {
            .path = path
        });

        procs.PushBack([this, asset_index]() {
            LoadObjectWrapper<T>()(asset_manager, *this, asset_index);
        });
    }

    /*! \brief Begin loading this batch asynchronously. Note that
        you may not add any more tasks to be loaded once you call this method. */
    void LoadAsync(UInt num_batches = UINT_MAX);
    DynArray<EnqueuedAsset> AwaitResults();
    DynArray<EnqueuedAsset> ForceLoad();

private:
    AssetManager &asset_manager;
    DynArray<Proc<void>> procs;
};

} // namespace hyperion::V2

#endif