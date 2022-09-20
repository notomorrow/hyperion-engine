#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void AssetBatch::LoadAsync()
{
    if (enqueued_assets.Empty()) {
        return;
    }

    asset_manager.m_engine->task_system.EnqueueBatch(this);
}

DynArray<EnqueuedAsset> AssetBatch::AwaitResults()
{
    AwaitCompletion();

    auto results = std::move(enqueued_assets);

    // enqueued_assets is cleared now

    return results;
}

} // namespace hyperion::v2