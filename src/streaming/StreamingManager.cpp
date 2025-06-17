/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamingManager.hpp>
#include <streaming/StreamingCell.hpp>
#include <streaming/StreamingCellCollection.hpp>
#include <streaming/StreamingVolume.hpp>

#include <scene/world_grid/WorldGrid.hpp>
#include <scene/world_grid/WorldGridLayer.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/containers/Queue.hpp>

#include <core/object/HypClass.hpp>

#include <core/math/MathUtil.hpp>

#include <core/memory/MemoryPool.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/algorithm/Map.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/octree/Octree.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

#pragma region Helpers

static const FixedArray<StreamingCellNeighbor, 8> GetCellNeighbors(const Vec2i& coord)
{
    return {
        StreamingCellNeighbor { coord + Vec2i { 1, 0 } },
        StreamingCellNeighbor { coord + Vec2i { -1, 0 } },
        StreamingCellNeighbor { coord + Vec2i { 0, 1 } },
        StreamingCellNeighbor { coord + Vec2i { 0, -1 } },
        StreamingCellNeighbor { coord + Vec2i { 1, -1 } },
        StreamingCellNeighbor { coord + Vec2i { -1, -1 } },
        StreamingCellNeighbor { coord + Vec2i { 1, 1 } },
        StreamingCellNeighbor { coord + Vec2i { -1, 1 } }
    };
}

static Vec2i WorldSpaceToCellCoord(const WorldGridLayerInfo& layerInfo, const Vec3f& worldPosition)
{
    Vec3f scaled = worldPosition - layerInfo.offset;
    scaled *= Vec3f::One() / (layerInfo.scale * (Vec3f(layerInfo.cellSize) - 1.0f));
    scaled = MathUtil::Floor(scaled);

    return { int(scaled.x), int(scaled.z) };
}

#pragma endregion Helpers

#pragma region StreamingWorkerThread

class StreamingWorkerThread : public TaskThread
{
public:
    StreamingWorkerThread(ThreadId id)
        : TaskThread(id, ThreadPriorityValue::LOW)
    {
    }

    virtual ~StreamingWorkerThread() override = default;
};

#pragma endregion StreamingWorkerThread

#pragma region StreamingThreadPool

class StreamingThreadPool : public TaskThreadPool
{
public:
    StreamingThreadPool()
        : TaskThreadPool(TypeWrapper<StreamingWorkerThread>(), "StreamingWorker", 2)
    {
    }

    virtual ~StreamingThreadPool() override = default;
};

#pragma endregion StreamingThreadPool

#pragma region StreamingManagerThread

class StreamingManagerThread final : public Thread<Scheduler, StreamingManager*>
{
    struct LayerData
    {
        enum LayerDataFlags
        {
            LDF_NONE = 0x0,
            LDF_PENDING_REMOVAL = 0x2
        };

        Handle<WorldGridLayer> layer;
        StreamingCellCollection cells;
        Queue<StreamingCellUpdate> cellUpdateQueue;
        AtomicVar<uint8> flags { LDF_NONE };
        AtomicVar<uint32> lockCount { 0 };

        LayerData(const Handle<WorldGridLayer>& layer)
            : layer(layer)
        {
            AssertThrow(layer.IsValid());
        }

        void Lock()
        {
            lockCount.Increment(1, MemoryOrder::RELEASE);
        }

        void Unlock()
        {
            uint32 value = lockCount.Decrement(1, MemoryOrder::RELEASE);
            AssertDebugMsg(value > 0, "Lock count cannot be negative!");
        }

        bool IsLocked() const
        {
            return lockCount.Get(MemoryOrder::ACQUIRE) > 0;
        }

        void SetPendingRemoval()
        {
            flags.BitOr(LDF_PENDING_REMOVAL, MemoryOrder::RELEASE);
        }

        bool IsPendingRemoval() const
        {
            return flags.Get(MemoryOrder::ACQUIRE) & LDF_PENDING_REMOVAL;
        }
    };

public:
    friend class StreamingManager;

    StreamingManagerThread()
        : Thread(ThreadId(Name::Unique("StreamingManagerThread")), ThreadPriorityValue::NORMAL),
          m_threadPool(MakeUnique<StreamingThreadPool>())
    {
    }

    virtual ~StreamingManagerThread() override
    {
        for (const Handle<StreamingVolumeBase>& volume : m_volumes)
        {
            if (volume.IsValid())
            {
                volume->UnregisterNotifier(&m_notifier);
            }
        }
    }

    void AddStreamingVolume(const Handle<StreamingVolumeBase>& volume)
    {
        if (!volume.IsValid())
        {
            return;
        }

        if (!IsRunning() || Threads::IsOnThread(Id()))
        {
            m_volumes.PushBack(volume);
        }
        else
        {
            m_scheduler.Enqueue([this, volume = volume]()
                {
                    m_volumes.PushBack(volume);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            m_notifier.Produce(1);
        }
    }

    void RemoveStreamingVolume(const StreamingVolumeBase* volume)
    {
        if (!IsRunning() || Threads::IsOnThread(Id()))
        {
            auto it = m_volumes.FindAs(volume);
            AssertThrowMsg(it != m_volumes.End(), "StreamingVolume not found in streaming manager!");

            m_volumes.Erase(it);
        }
        else
        {
            m_scheduler.Enqueue([this, volume]()
                {
                    auto it = m_volumes.FindAs(volume);
                    AssertThrowMsg(it != m_volumes.End(), "StreamingVolume not found in streaming manager!");

                    m_volumes.Erase(it);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            m_notifier.Produce(1);
        }
    }

    void AddWorldGridLayer(const Handle<WorldGridLayer>& layer)
    {
        if (!layer.IsValid())
        {
            return;
        }

        if (!IsRunning() || Threads::IsOnThread(Id()))
        {
            auto it = m_layers.FindIf([layer](const LayerData& data)
                {
                    return data.layer == layer;
                });

            AssertThrowMsg(it == m_layers.End(), "WorldGridLayer already exists in streaming manager!");

            m_layers.EmplaceBack(layer);
        }
        else
        {
            m_scheduler.Enqueue([this, layer = layer]()
                {
                    auto it = m_layers.FindIf([layer](const LayerData& data)
                        {
                            return data.layer == layer;
                        });

                    AssertThrowMsg(it == m_layers.End(), "WorldGridLayer already exists in streaming manager!");

                    m_layers.EmplaceBack(layer);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            m_notifier.Produce(1);
        }
    }

    void RemoveWorldGridLayer(const WorldGridLayer* layer)
    {
        if (!IsRunning() || Threads::IsOnThread(Id()))
        {
            auto it = m_layers.FindIf([layer](const LayerData& data)
                {
                    return data.layer == layer;
                });

            AssertThrowMsg(it != m_layers.End(), "WorldGridLayer not found in streaming manager!");

            if (it->IsLocked())
            {
                it->SetPendingRemoval();
                return;
            }

            m_layers.Erase(it);
        }
        else
        {
            m_scheduler.Enqueue([this, layer]()
                {
                    auto it = m_layers.FindIf([layer](const LayerData& data)
                        {
                            return data.layer == layer;
                        });

                    AssertThrowMsg(it != m_layers.End(), "WorldGridLayer not found in streaming manager!");

                    if (it->IsLocked())
                    {
                        it->SetPendingRemoval();
                        return;
                    }

                    m_layers.Erase(it);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            m_notifier.Produce(1);
        }
    }

    void SinkGameThreadUpdates(Array<Pair<Handle<StreamingCell>, StreamingCellState>>& out)
    {
        Threads::AssertOnThread(g_gameThread);

        out.Concat(std::move(m_cellUpdatesGameThread));
    }

    HYP_FORCE_INLINE StreamingNotifier& GetNotifier()
    {
        return m_notifier;
    }

    void Stop()
    {
        m_threadPool->Stop();

        m_stopRequested.Set(true, MemoryOrder::RELAXED);
        m_notifier.Produce(1); // Wake up the thread if it's waiting on the notifier.
    }

private:
    virtual void operator()(StreamingManager* streamingManager) override
    {
        for (const Handle<StreamingVolumeBase>& volume : m_volumes)
        {
            InitObject(volume);
        }

        for (const LayerData& layerData : m_layers)
        {
            InitObject(layerData.layer);
        }

        StartWorkerThreadPool();

        // Set the notifier to the initial value of 1 so it won't block the first call.
        m_notifier.Produce(1);

        while (!m_stopRequested.Get(MemoryOrder::RELAXED))
        {
            m_notifier.Acquire();

            int32 num = m_notifier.GetValue();

            do
            {
                DoWork(streamingManager);

                num = m_notifier.Release(num);

                AssertDebug(num >= 0); // sanity check
            }
            while (num > 0 && !m_stopRequested.Get(MemoryOrder::RELAXED));

            Threads::Sleep(1000);
        }
    }

    void StartWorkerThreadPool();
    void DoWork(StreamingManager* streamingManager);
    void ProcessCellUpdatesForLayer(LayerData& layerData);
    void GetDesiredCellsForLayer(const LayerData& layerData, const Handle<StreamingVolumeBase>& volume, HashSet<Vec2i>& outCellCoords) const;

    void PostCellUpdateToGameThread(Handle<StreamingCell> cell, StreamingCellState state)
    {
        Mutex::Guard guard(m_gameThreadFuturesMutex);

        Task<void>& future = m_gameThreadFutures.EmplaceBack();
        TaskPromise<void>* promise = future.Promise();

        Threads::GetThread(g_gameThread)->GetScheduler().Enqueue([this, promise, cell = std::move(cell), state]()
            {
                m_cellUpdatesGameThread.EmplaceBack(std::move(cell), state);

                promise->Fulfill();

                Mutex::Guard guard(m_gameThreadFuturesMutex);

                auto it = m_gameThreadFutures.FindIf([promise](const Task<void>& task)
                    {
                        return task.GetTaskExecutor() == promise;
                    });

                AssertThrowMsg(it != m_gameThreadFutures.End(), "Task not found in game thread tasks!");

                m_gameThreadFutures.Erase(it);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    UniquePtr<StreamingThreadPool> m_threadPool;

    Array<Handle<StreamingVolumeBase>> m_volumes;
    LinkedList<LayerData> m_layers;

    Array<Pair<Handle<StreamingCell>, StreamingCellState>> m_cellUpdatesGameThread;
    LinkedList<Task<void>> m_gameThreadFutures;
    Mutex m_gameThreadFuturesMutex;

    StreamingNotifier m_notifier;
};

void StreamingManagerThread::StartWorkerThreadPool()
{
    AssertThrow(m_threadPool != nullptr);
    AssertThrow(!m_threadPool->IsRunning());

    m_threadPool->Start();

    while (!m_threadPool->IsRunning())
    {
        Threads::Sleep(0);
    }
}

void StreamingManagerThread::DoWork(StreamingManager* streamingManager)
{
    Queue<Scheduler::ScheduledTask> tasks;

    if (uint32 numEnqueued = m_scheduler.NumEnqueued())
    {
        m_scheduler.AcceptAll(tasks);

        while (tasks.Any())
        {
            tasks.Pop().Execute();
        }
    }

    // HYP_LOG(Streaming, Debug, "Processing streaming work on thread: {}, {} layers, {} volumes, {} cells",
    //     Threads::CurrentThreadId().GetName(),
    //     m_layers.Size(),
    //     m_volumes.Size(),
    //     String::Join(
    //         Map(
    //             m_layers,
    //             [](const LayerData& layerData)
    //             {
    //                 return HYP_FORMAT("Layer: #{} : {}", layerData.layer->Id().Value(), layerData.cells.Size());
    //             }),
    //         ", "));

    for (auto it = m_layers.Begin(); it != m_layers.End();)
    {
        LayerData& layerData = *it;

        if (layerData.IsLocked())
        {
            HYP_LOG(Streaming, Debug, "Layer {} is locked, skipping processing", layerData.layer->GetLayerInfo().gridSize);

            ++it;

            continue;
        }

        if (layerData.IsPendingRemoval())
        {
            HYP_LOG(Streaming, Debug, "Layer {} is pending removal, erasing", layerData.layer->GetLayerInfo().gridSize);

            it = m_layers.Erase(it);

            continue;
        }

        const Handle<WorldGridLayer>& layer = layerData.layer;
        AssertThrow(layer.IsValid());

        StreamingCellCollection& cells = layerData.cells;
        Queue<StreamingCellUpdate>& cellUpdateQueue = layerData.cellUpdateQueue;

        HYP_LOG(Streaming, Debug, "Processing layer: {} {}", layer->GetLayerInfo().gridSize, layer->GetLayerInfo().cellSize);

        const WorldGridLayerInfo& layerInfo = layer->GetLayerInfo();

        HashSet<Vec2i> desiredCells;

        for (const Handle<StreamingVolumeBase>& volume : m_volumes)
        {
            if (!volume.IsValid())
            {
                continue;
            }

            GetDesiredCellsForLayer(layerData, volume, desiredCells);
        }

        // @TODO Use bitset via IDs, or by cell index (x * height + y, would need constant max dimensions for that) to track desired cells and undesired cells.
        Array<Vec2i> cellsToAdd = desiredCells.ToArray();
        Array<Handle<StreamingCell>> cellsToRemove;

        for (const StreamingCellRuntimeInfo& cellRuntimeInfo : cells)
        {
            auto it = desiredCells.Find(cellRuntimeInfo.coord);

            if (it == desiredCells.End())
            {
                AssertThrow(cellRuntimeInfo.cell.IsValid());

                // Lock so we can use it safely in the loop below for pushing to queue.
                if (!cells.SetCellLockState(cellRuntimeInfo.coord, true))
                {
                    // Already locked, skip adding for removal
                    continue;
                }

                cellsToRemove.PushBack(cellRuntimeInfo.cell);
            }
            else
            {
                // Already have the cell
                cellsToAdd.Erase(cellRuntimeInfo.coord);
            }
        }

        if (cellsToRemove.Any())
        {
            for (const Handle<StreamingCell>& cell : cellsToRemove)
            {
                AssertThrow(cell.IsValid());
                AssertDebugMsg(cells.IsCellLocked(cell->GetPatchInfo().coord),
                    "StreamingCell with coord %d,%d is not locked for unloading!",
                    cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y);

                // Cell is locked here -- request unloading.
                cellUpdateQueue.Push(StreamingCellUpdate { cell->GetPatchInfo().coord, StreamingCellState::UNLOADING });
            }
        }

        if (cellsToAdd.Any())
        {
            for (const Vec2i& coord : cellsToAdd)
            {
                AssertThrowMsg(!cells.HasCell(coord), "StreamingCell with coord %d,%d already exists!", coord.x, coord.y);

                cellUpdateQueue.Push(StreamingCellUpdate { coord, StreamingCellState::WAITING });
            }
        }

        ProcessCellUpdatesForLayer(layerData);

        ++it;
    }
}

void StreamingManagerThread::ProcessCellUpdatesForLayer(LayerData& layerData)
{
    const WorldGridLayerInfo& layerInfo = layerData.layer->GetLayerInfo();
    StreamingCellCollection& cells = layerData.cells;
    Queue<StreamingCellUpdate>& cellUpdateQueue = layerData.cellUpdateQueue;

    if (cellUpdateQueue.Empty())
    {
        return;
    }

    LinkedList<Proc<void()>> deferredUpdates;

    while (cellUpdateQueue.Any())
    {
        StreamingCellUpdate update = cellUpdateQueue.Pop();

        HYP_LOG(Streaming, Debug, "Processing StreamingCellUpdate for coord: {}, state: {}", update.coord, update.state);

        switch (update.state)
        {
        case StreamingCellState::WAITING:
        {
            AssertThrowMsg(!cells.HasCell(update.coord), "StreamingCell with coord %d,%d already exists!", update.coord.x, update.coord.y);

            StreamingCellInfo cellInfo;
            cellInfo.coord = update.coord;
            cellInfo.extent = layerInfo.cellSize;
            cellInfo.scale = layerInfo.scale;
            cellInfo.bounds.min = {
                layerInfo.offset.x + (float(cellInfo.coord.x) - 0.5f) * (float(cellInfo.extent.x) - 1.0f) * cellInfo.scale.x,
                layerInfo.offset.y,
                layerInfo.offset.z + (float(cellInfo.coord.y) - 0.5f) * (float(cellInfo.extent.y) - 1.0f) * cellInfo.scale.z
            };
            cellInfo.bounds.max = cellInfo.bounds.min + Vec3f(cellInfo.extent) * cellInfo.scale;

            Handle<StreamingCell> cell = layerData.layer->CreateStreamingCell(cellInfo);

            if (!cell.IsValid())
            {
                HYP_LOG(Streaming, Error, "Failed to create StreamingCell for coord: {}", update.coord);
                continue;
            }

            InitObject(cell);

            const bool wasCellAdded = cells.AddCell(cell, StreamingCellState::WAITING, /* lock */ true);
            AssertThrowMsg(wasCellAdded, "Failed to add StreamingCell with coord: %d,%d", update.coord.x, update.coord.y);

            PostCellUpdateToGameThread(cell, StreamingCellState::WAITING);

            layerData.Lock();

            deferredUpdates.EmplaceBack([this, &layerData, cell]()
                {
                    HYP_LOG(Streaming, Debug, "Loading StreamingCell at coord: {} on thread: {} for layer: {}",
                        cell->GetPatchInfo().coord, Threads::CurrentThreadId().GetName(), layerData.layer->InstanceClass()->GetName());

                    bool isOk = true;

                    isOk &= layerData.cells.UpdateCellState(cell->GetPatchInfo().coord, StreamingCellState::LOADING);
                    AssertDebugMsg(isOk, "Failed to update StreamingCell state to LOADING for coord: %d,%d for layer: %s",
                        cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y,
                        layerData.layer->InstanceClass()->GetName().LookupString());

                    PostCellUpdateToGameThread(cell, StreamingCellState::LOADING);

                    cell->OnStreamStart();

                    isOk &= layerData.cells.UpdateCellState(cell->GetPatchInfo().coord, StreamingCellState::LOADED);
                    AssertDebugMsg(isOk, "Failed to update StreamingCell state to LOADED for coord: %d,%d for layer: %s\tCurrent state: %u",
                        cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y,
                        layerData.layer->InstanceClass()->GetName().LookupString(),
                        layerData.cells.GetCellState(cell->GetPatchInfo().coord));

                    isOk &= layerData.cells.SetCellLockState(cell->GetPatchInfo().coord, false);
                    AssertDebugMsg(isOk, "Failed to unlock StreamingCell with coord: %d,%d for layer: %s",
                        cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y,
                        layerData.layer->InstanceClass()->GetName().LookupString());

                    PostCellUpdateToGameThread(cell, StreamingCellState::LOADED);

                    layerData.Unlock();
                });

            break;
        }
        case StreamingCellState::UNLOADING:
        {
            bool isOk = true;

            isOk &= cells.HasCell(update.coord);
            AssertThrowMsg(isOk, "StreamingCell with coord %d,%d does not exist!", update.coord.x, update.coord.y);

            // Locked here - see StreamingManagerThread::DoWork where we lock before pushing UNLOADING state.

            isOk &= cells.IsCellLocked(update.coord);
            AssertThrowMsg(isOk, "StreamingCell with coord %d,%d for layer %s is not locked for unloading!",
                update.coord.x, update.coord.y, layerData.layer->InstanceClass()->GetName().LookupString());

            Handle<StreamingCell> cell = cells.GetCell(update.coord);
            AssertThrowMsg(cell.IsValid(), "StreamingCell with coord %d,%d for layer %s is not valid!",
                update.coord.x, update.coord.y, layerData.layer->InstanceClass()->GetName().LookupString());

            isOk &= cells.UpdateCellState(cell->GetPatchInfo().coord, StreamingCellState::UNLOADING);
            AssertDebugMsg(isOk, "Failed to update StreamingCell state to UNLOADING for coord: %d,%d for layer: %s",
                cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y,
                layerData.layer->InstanceClass()->GetName().LookupString());

            PostCellUpdateToGameThread(cell, StreamingCellState::UNLOADING);

            isOk &= cells.RemoveCell(cell->GetPatchInfo().coord);
            AssertDebugMsg(isOk, "Failed to remove StreamingCell with coord: %d,%d for layer: %s",
                cell->GetPatchInfo().coord.x, cell->GetPatchInfo().coord.y,
                layerData.layer->InstanceClass()->GetName().LookupString());

            HYP_LOG(Streaming, Debug, "Removed StreamingCell at coord: {} for layer: {} on thread: {}",
                cell->GetPatchInfo().coord, layerData.layer->InstanceClass()->GetName().LookupString(),
                Threads::CurrentThreadId().GetName());

            layerData.Lock();

            // Call OnStreamEnd on the cell and then Unload it
            deferredUpdates.EmplaceBack([this, cell = std::move(cell), &layerData]()
                {
                    HYP_LOG(Streaming, Debug, "Unloading StreamingCell at coord: {} for layer: {} on thread: {}",
                        cell->GetPatchInfo().coord, layerData.layer->InstanceClass()->GetName().LookupString(),
                        Threads::CurrentThreadId().GetName());

                    // cell->OnStreamEnd();

                    PostCellUpdateToGameThread(cell, StreamingCellState::UNLOADED);

                    layerData.Unlock();
                });

            break;
        }
        default:
        {
            break;
        }
        }
    }

    if (deferredUpdates.Any())
    {
        for (auto it = deferredUpdates.Begin(); it != deferredUpdates.End(); ++it)
        {
            TaskSystem::GetInstance().Enqueue(std::move(*it), *m_threadPool, TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
}

void StreamingManagerThread::GetDesiredCellsForLayer(const LayerData& layerData, const Handle<StreamingVolumeBase>& volume, HashSet<Vec2i>& outCellCoords) const
{
    constexpr Vec2i cellNeighborDirections[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

    const WorldGridLayerInfo& layerInfo = layerData.layer->GetLayerInfo();

    BoundingBox aabb;

    if (!volume->GetBoundingBox(aabb))
    {
        return;
    }

    Queue<Vec2f> queue;
    HashSet<Vec2i> visited;

    const Vec2f centerCoord = Vec2f(WorldSpaceToCellCoord(layerInfo, aabb.GetCenter()));

    queue.Push(centerCoord);
    visited.Insert(Vec2i(centerCoord));

    const float maxDistSq = layerInfo.maxDistance * layerInfo.maxDistance;

    while (queue.Any())
    {
        const Vec2f current = queue.Pop();

        // euclidean distance check
        if (Vec2f(current).DistanceSquared(centerCoord) > maxDistSq)
        {
            continue;
        }

        outCellCoords.Insert(Vec2i(current));

        for (const Vec2i dir : cellNeighborDirections)
        {
            const Vec2f neighbor = current + Vec2f(dir);

            if (visited.Insert(Vec2i(neighbor)).second)
            {
                queue.Push(neighbor);
            }
        }
    }
}

#pragma endregion StreamingManagerThread

#pragma region StreamingManager

StreamingManager::StreamingManager()
    : StreamingManager(WeakHandle<WorldGrid>())
{
}

StreamingManager::StreamingManager(const WeakHandle<WorldGrid>& worldGrid)
    : m_worldGrid(worldGrid),
      m_thread(MakeUnique<StreamingManagerThread>())
{
}

StreamingManager::~StreamingManager()
{
    Stop();
}

void StreamingManager::AddStreamingVolume(const Handle<StreamingVolumeBase>& volume)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    AssertThrow(volume.IsValid());

    volume->RegisterNotifier(&m_thread->GetNotifier());

    m_thread->AddStreamingVolume(volume);
}

void StreamingManager::RemoveStreamingVolume(StreamingVolumeBase* volume)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!volume)
    {
        return;
    }

    volume->UnregisterNotifier(&m_thread->GetNotifier());

    m_thread->RemoveStreamingVolume(volume);
}

void StreamingManager::AddWorldGridLayer(const Handle<WorldGridLayer>& layer)
{
    HYP_SCOPE;
    // Threads::AssertOnThread(g_gameThread);

    AssertThrow(layer.IsValid());

    m_thread->AddWorldGridLayer(layer);
}

void StreamingManager::RemoveWorldGridLayer(WorldGridLayer* layer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!layer)
    {
        return;
    }

    m_thread->RemoveWorldGridLayer(layer);
}

void StreamingManager::Start()
{
    if (!m_thread->IsRunning())
    {
        m_thread = MakeUnique<StreamingManagerThread>();

        if (!m_thread->Start(this))
        {
            HYP_FAIL("Failed to start StreamingManagerThread!");
        }
    }
}

void StreamingManager::Stop()
{
    if (m_thread->IsRunning())
    {
        m_thread->Stop();
        m_thread.Reset();
    }
}

void StreamingManager::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            Stop();
        }));

    SetReady(true);
}

void StreamingManager::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    Array<Pair<Handle<StreamingCell>, StreamingCellState>> updates;
    m_thread->SinkGameThreadUpdates(updates);

    if (updates.Empty())
    {
        return;
    }

    for (Pair<Handle<StreamingCell>, StreamingCellState>& update : updates)
    {
        Handle<StreamingCell> cell = std::move(update.first);
        AssertThrowMsg(cell.IsValid(), "StreamingCell is not valid!");

        switch (update.second)
        {
        case StreamingCellState::LOADED:
            cell->OnLoaded();
            break;
        case StreamingCellState::UNLOADED:
            cell->OnRemoved();
            break;
        default:
            break;
        }
    }
}

#pragma endregion StreamingManager

} // namespace hyperion