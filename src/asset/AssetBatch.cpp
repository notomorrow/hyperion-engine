/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <asset/AssetBatch.hpp>
#include <asset/Assets.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region AssetBatch

AssetBatch::AssetBatch(AssetManager* assetManager)
    : TaskBatch(),
      m_assetMap(MakeUnique<AssetMap>()),
      m_assetManager(assetManager)
{
    AssertThrow(assetManager != nullptr);
}

void AssetBatch::LoadAsync(uint32 numBatches)
{
    HYP_SCOPE;

    AssertThrowMsg(m_assetMap != nullptr, "AssetBatch is in invalid state");
    AssertThrowMsg(m_assetManager != nullptr, "AssetBatch is in invalid state");

    // Set pool to use the asset manager's thread pool if one hasn't been set to override it.
    if (!TaskBatch::pool)
    {
        if (TaskThreadPool* threadPool = m_assetManager->GetThreadPool())
        {
            pool = threadPool;
        }
    }

    m_results.Clear();

    if (m_assetMap->Empty())
    {
        return;
    }

    // partition each proc
    AssertThrow(m_procs.Size() == m_assetMap->Size());

    const uint32 numItems = uint32(m_procs.Size());

    numBatches = MathUtil::Max(numBatches, 1u);
    numBatches = MathUtil::Min(numBatches, numItems);

    const uint32 itemsPerBatch = numItems / numBatches;

    m_results.Resize(numBatches);

    for (uint32 batchIndex = 0; batchIndex < numBatches; batchIndex++)
    {
        const uint32 offsetIndex = batchIndex * itemsPerBatch;

        const uint32 maxIndex = MathUtil::Min(offsetIndex + itemsPerBatch, numItems);
        AssertThrow(maxIndex >= offsetIndex);

        Array<UniquePtr<ProcessAssetFunctorBase>, DynamicAllocator> batchProcs;
        batchProcs.Reserve(maxIndex - offsetIndex);

        for (uint32 i = offsetIndex; i < maxIndex; ++i)
        {
            AssertThrow(i < m_procs.Size());
            AssertThrow(m_procs[i] != nullptr);
            batchProcs.PushBack(std::move(m_procs[i]));
        }

        AddTask([this, batchProcs = std::move(batchProcs), batchIndex]() mutable -> void
            {
                HYP_NAMED_SCOPE("Processing assets in batch");

                Array<TResult<void, AssetLoadError>> batchResults;
                batchResults.Reserve(batchProcs.Size());

                for (auto& proc : batchProcs)
                {
                    batchResults.PushBack((*proc)(*m_assetManager, *m_assetMap));
                }

                m_results[batchIndex] = std::move(batchResults);
            });
    }

    m_procs.Clear();

    TaskSystem::GetInstance().EnqueueBatch(this);

    m_assetManager->AddPendingBatch(RefCountedPtrFromThis());
}

AssetMap AssetBatch::AwaitResults()
{
    AssertThrowMsg(m_assetMap != nullptr, "AssetBatch is in invalid state");

    AwaitCompletion();

    // @TODO Handle m_results

    return std::move(*m_assetMap);
}

AssetMap AssetBatch::ForceLoad()
{
    AssertThrowMsg(m_assetMap != nullptr, "AssetBatch is in invalid state");

    for (auto& proc : m_procs)
    {
        (*proc)(*m_assetManager, *m_assetMap);
    }

    m_procs.Clear();

    return std::move(*m_assetMap);
}

void AssetBatch::Add(const String& key, const String& path)
{
    AssertThrowMsg(IsCompleted(), "Cannot add assets while loading!");
    AssertThrowMsg(m_assetMap != nullptr, "AssetBatch is in invalid state");

    if (!m_assetMap->Emplace(key).second)
    {
        return;
    }

    UniquePtr<ProcessAssetFunctorBase> functorPtr = m_assetManager->CreateProcessAssetFunctor(
        key,
        path,
        &m_callbacks);

    AssertThrowMsg(
        functorPtr != nullptr,
        "Failed to create ProcessAssetFunctor - perhaps the asset type is not registered or the path is invalid");

    m_procs.PushBack(std::move(functorPtr));
}

#pragma endregion AssetBatch

#pragma region AssetManager

UniquePtr<ProcessAssetFunctorBase> AssetManager::CreateProcessAssetFunctor(TypeId loaderTypeId, const String& key, const String& path, AssetBatchCallbacks* callbacksPtr)
{
    auto it = m_functorFactories.Find(loaderTypeId);
    AssertThrow(it != m_functorFactories.End());

    return it->second(key, path, callbacksPtr);
}

#pragma endregion AssetManager

} // namespace hyperion