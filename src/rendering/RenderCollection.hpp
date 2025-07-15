/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/object/ObjId.hpp>

#include <core/math/Transform.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderStats.hpp>
#include <rendering/IndirectDraw.hpp>

#include <rendering/RenderQueue.hpp>

#include <rendering/RenderStructs.hpp>
#include <rendering/RenderObject.hpp>

#include <scene/RenderProxyable.hpp>

#include <Types.hpp>

namespace hyperion {

class Scene;
class Camera;
class Entity;
class RenderGroup;
class RenderCamera;
class RenderView;
struct RenderSetup;
class IndirectRenderer;
class RenderLight;
class LightmapVolume;
class RenderEnvGrid;
class RenderEnvProbe;
class ReflectionProbe;
class Texture;
class Skeleton;
class RenderCollector;
struct ResourceContainer;
enum class RenderGroupFlags : uint32;
enum LightType : uint32;
enum EnvProbeType : uint32;

struct ParallelRenderingState
{
    static constexpr uint32 maxBatches = g_numAsyncRenderingCommandBuffers;

    TaskBatch* taskBatch = nullptr;

    uint32 numBatches = 0;

    // Non-async rendering command list - used for binding state at the start of the pass before async stuff
    RenderQueue rootQueue;

    // per-thread RenderQueue
    FixedArray<RenderQueue, maxBatches> localQueues {};

    FixedArray<RenderStatsCounts, maxBatches> renderStatsCounts {};

    // Temporary storage for data that will be executed in parallel during the frame
    Array<Span<const DrawCall>, FixedAllocator<maxBatches>> drawCalls;
    Array<Span<const InstancedDrawCall>, FixedAllocator<maxBatches>> instancedDrawCalls;
    Array<Proc<void(Span<const DrawCall>, uint32, uint32)>, FixedAllocator<1>> drawCallProcs;
    Array<Proc<void(Span<const InstancedDrawCall>, uint32, uint32)>, FixedAllocator<1>> instancedDrawCallProcs;

    ParallelRenderingState* next = nullptr;
};

// Utility struct that maps attribute sets -> draw call collections that have been written to already and had render groups created.
struct DrawCallCollectionMapping
{
    Handle<RenderGroup> renderGroup;
    // map entity id to mesh proxy
    SparsePagedArray<RenderProxyMesh*, 128> meshProxies;
    DrawCallCollection drawCallCollection;
    IndirectRenderer* indirectRenderer = nullptr;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return renderGroup.IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return renderGroup.IsValid();
    }
};

/*! \brief A collection of renderable objects and setup for a specific View, proxied so the render thread can work with it. */
struct HYP_API RenderProxyList
{
    static constexpr uint64 writeFlag = 0x1;
    static constexpr uint64 readMask = uint64(-1) & ~writeFlag;

    RenderProxyList();

    RenderProxyList(const RenderProxyList& other) = delete;
    RenderProxyList& operator=(const RenderProxyList& other) = delete;

    RenderProxyList(RenderProxyList&& other) noexcept = delete;
    RenderProxyList& operator=(RenderProxyList&& other) noexcept = delete;

    ~RenderProxyList();

    void BeginWrite()
    {
        uint64 rwMarkerState = rwMarker.BitOr(writeFlag, MemoryOrder::ACQUIRE);
        while (HYP_UNLIKELY(rwMarkerState & readMask))
        {
            HYP_LOG_TEMP("Busy waiting for read marker to be released! "
                         "If this is occuring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

            rwMarkerState = rwMarker.Get(MemoryOrder::ACQUIRE);
            HYP_WAIT_IDLE();
        }

        AssertDebug(state != CS_READING, "Got state == CS_READING, race condition! (rwMarkerState = {})", rwMarkerState);
        state = CS_WRITING;
    }

    void EndWrite()
    {
        AssertDebug(state == CS_WRITING);
        state = CS_WRITTEN;

        rwMarker.BitAnd(~writeFlag, MemoryOrder::RELEASE);
    }

    void BeginRead()
    {
        uint64 rwMarkerState;

        do
        {
            rwMarkerState = rwMarker.Increment(2, MemoryOrder::ACQUIRE);

            if (HYP_UNLIKELY(rwMarkerState & writeFlag))
            {
                HYP_LOG_TEMP("Waiting for write marker to be released."
                             "If this is occurring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

                rwMarker.Decrement(2, MemoryOrder::RELAXED);

                // spin to wait for write flag to be released
                HYP_WAIT_IDLE();
            }
        }
        while (HYP_UNLIKELY(rwMarkerState & writeFlag));

        AssertDebug(state != CS_WRITING);
        state = CS_READING;
    }

    void EndRead()
    {
        AssertDebug(state == CS_READING);

        uint64 rwMarkerState = rwMarker.Decrement(2, MemoryOrder::ACQUIRE_RELEASE);
        AssertDebug(rwMarkerState & readMask, "Invalid state, expected read mask to be set when calling EndRead()");

        /// FIXME: If BeginRead() is called on other thread between the check and setting state to CS_DONE,
        /// we could set state to done when it isn't actually.
        if (((rwMarkerState - 2) & readMask) == 0)
        {
            state = CS_DONE;
        }
    }

    // State for tracking transitions from writing (game thread) to reading (render thread).
    enum CollectionState : uint8
    {
        CS_WRITING, //!< Currently being written to. set when the frame starts on the game thread.
        CS_WRITTEN, //!< Written to, but not yet read from. set when the frame finishes on the game thread.
        CS_READING, //!< Currently ready to be read. set when the frame starts on the render thread.
        CS_DONE     //!< Finished reading. set when the frame finishes on the render thread.
    };

    CollectionState state : 2 = CS_DONE;

    // are mesh entities sorted using an indirect array to map sort order?
    bool useOrdering : 1 = false;
    bool TEMP_disableBuildRenderCollection : 1 = false;

    Viewport viewport;
    int priority;

    ResourceTracker<ObjId<Entity>, Entity*, RenderProxyMesh> meshes;
    ResourceTracker<ObjId<EnvProbe>, EnvProbe*, RenderProxyEnvProbe> envProbes;
    ResourceTracker<ObjId<Light>, Light*, RenderProxyLight> lights;
    ResourceTracker<ObjId<EnvGrid>, EnvGrid*, RenderProxyEnvGrid> envGrids;
    ResourceTracker<ObjId<LightmapVolume>, LightmapVolume*, RenderProxyLightmapVolume> lightmapVolumes;
    ResourceTracker<ObjId<Material>, Material*, RenderProxyMaterial> materials;
    ResourceTracker<ObjId<Skeleton>, Skeleton*, RenderProxySkeleton> skeletons;
    ResourceTracker<ObjId<Texture>, Texture*> textures;

    Array<Pair<ObjId<Entity>, int>> meshEntityOrdering;

    // marker to set to locked when game thread is writing to this list.
    // this only really comes into play with non-buffered Views that do not double/triple buffer their RenderProxyLists
    AtomicVar<uint64> rwMarker { 0 };

#ifdef HYP_ENABLE_MT_CHECK
    HYP_DECLARE_MT_CHECK(dataRaceDetector);
#endif
};

class HYP_API RenderCollector
{
public:
    RenderCollector();
    RenderCollector(const RenderCollector& other) = delete;
    RenderCollector& operator=(const RenderCollector& other) = delete;
    RenderCollector(RenderCollector&& other) noexcept = delete;
    RenderCollector& operator=(RenderCollector&& other) noexcept = delete;
    ~RenderCollector();

#ifdef HYP_DEBUG_MODE
    HYP_FORCE_INLINE SizeType NumDrawCallsCollected() const
    {
        SizeType numDrawCalls = 0;

        for (const auto& mappings : mappingsByBucket)
        {
            for (const KeyValuePair<RenderableAttributeSet, DrawCallCollectionMapping>& it : mappings)
            {
                const DrawCallCollectionMapping& mapping = it.second;

                numDrawCalls += mapping.drawCallCollection.drawCalls.Size()
                    + mapping.drawCallCollection.instancedDrawCalls.Size();
            }
        }

        return numDrawCalls;
    }
#endif

    void Clear(bool freeMemory = true);

    ParallelRenderingState* parallelRenderingStateHead;
    ParallelRenderingState* parallelRenderingStateTail;

    // map entity id to previous attribute set (for draw call collection)
    SparsePagedArray<RenderableAttributeSet, 128> previousAttributes;

    FixedArray<FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>, RB_MAX> mappingsByBucket;

    IDrawCallCollectionImpl* drawCallCollectionImpl;
    EnumFlags<RenderGroupFlags> renderGroupFlags;

    ParallelRenderingState* AcquireNextParallelRenderingState();
    void CommitParallelRenderingState(RenderQueue& renderQueue);

    void PerformOcclusionCulling(FrameBase* frame, const RenderSetup& renderSetup, uint32 bucketBits);

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, uint32 bucketBits);

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer, uint32 bucketBits);

    void RemoveEmptyRenderGroups();

    /*! \brief Counts the number of render groups in the list. */
    uint32 NumRenderGroups() const;

    /*! \brief Builds RenderGroups for proxies, based on renderable attributes */
    void BuildRenderGroups(View* view, const RenderProxyList& renderProxyList);

    void BuildDrawCalls(uint32 bucketBits);
};

template <class T>
static inline void UpdateRefs(T& renderProxyList)
{
    const auto impl = []<class ElementType, class ProxyType>(ResourceTracker<ObjId<ElementType>, ElementType*, ProxyType>& resourceTracker)
    {
        static const bool shouldUpdateRenderProxy = IsA<RenderProxyable, ElementType>();

        auto diff = resourceTracker.GetDiff();
        if (!diff.NeedsUpdate())
        {
            return;
        }

        Array<ElementType*> removed;
        resourceTracker.GetRemoved(removed, false);

        Array<ElementType*> added;
        resourceTracker.GetAdded(added, false);

        for (ElementType* resource : added)
        {
            resource->GetObjectHeader_Internal()->IncRefStrong();

            if constexpr (!std::is_same_v<ProxyType, NullProxy>)
            {
                ProxyType* pProxy = resourceTracker.GetProxy(ObjId<ElementType>(resource->Id()));

                if (!pProxy)
                {
                    ProxyType newProxy;
                    pProxy = resourceTracker.SetProxy(ObjId<ElementType>(resource->Id()), std::move(newProxy));
                }

                AssertDebug(pProxy != nullptr);

                if (shouldUpdateRenderProxy)
                {
                    HYP_LOG_TEMP("Update RenderProxy for {}", resource->Id());
                    static_cast<RenderProxyable*>(resource)->UpdateRenderProxy(pProxy);
                }
            }
        }

        for (ElementType* resource : removed)
        {
            resource->GetObjectHeader_Internal()->DecRefStrong();

            if constexpr (!std::is_same_v<ProxyType, NullProxy>)
            {
                resourceTracker.RemoveProxy(ObjId<ElementType>(resource->Id()));
            }
        }

        if constexpr (!std::is_same_v<ProxyType, NullProxy>)
        {
            if (shouldUpdateRenderProxy)
            {
                Array<ObjId<ElementType>> changedIds;
                resourceTracker.GetChanged(changedIds);

                for (const ObjId<ElementType>& id : changedIds)
                {
                    ElementType** ppResource = resourceTracker.GetElement(id);
                    AssertDebug(ppResource && *ppResource);

                    HYP_LOG_TEMP("Update RenderProxy for {}", id);

                    ElementType& resource = **ppResource;

                    ProxyType* pProxy = resourceTracker.GetProxy(id);
                    AssertDebug(pProxy != nullptr);

                    static_cast<RenderProxyable&>(resource).UpdateRenderProxy(pProxy);
                }
            }
        }
    };

    // impl(renderProxyList.meshes);
    impl(renderProxyList.lights);
    impl(renderProxyList.materials);
    impl(renderProxyList.skeletons);
    impl(renderProxyList.textures);
    impl(renderProxyList.lightmapVolumes);
    impl(renderProxyList.envProbes);
    impl(renderProxyList.envGrids);
}

} // namespace hyperion
