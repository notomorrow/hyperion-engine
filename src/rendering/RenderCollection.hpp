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

#include <rendering/rhi/CmdList.hpp>

#include <rendering/RenderStructs.hpp>
#include <rendering/RenderObject.hpp>

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
enum LightType : uint32;
enum EnvProbeType : uint32;

struct ParallelRenderingState
{
    static constexpr uint32 maxBatches = g_numAsyncRenderingCommandBuffers;

    TaskBatch* taskBatch = nullptr;

    uint32 numBatches = 0;

    // Non-async rendering command list - used for binding state at the start of the pass before async stuff
    CmdList baseCommandList;

    FixedArray<CmdList, maxBatches> commandLists {};
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

    void Clear();
    void RemoveEmptyRenderGroups();

    /*! \brief Counts the number of render groups in the list. */
    uint32 NumRenderGroups() const;

    /*! \brief Builds RenderGroups for proxies, based on renderable attributes */
    void BuildRenderGroups(View* view);

    ParallelRenderingState* AcquireNextParallelRenderingState();
    void CommitParallelRenderingState(CmdList& outCommandList);

    void BeginWrite()
    {
        uint64 rwMarkerState = rwMarker.BitOr(writeFlag, MemoryOrder::ACQUIRE);
        while (rwMarkerState & readMask)
        {
            HYP_LOG_TEMP("Waiting for read marker to be released."
                         "If this is occuring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

            rwMarkerState = rwMarker.Get(MemoryOrder::ACQUIRE);
            HYP_WAIT_IDLE();
        }

        AssertDebug(state != RenderProxyList::CS_READING);
        state = RenderProxyList::CS_WRITING;
    }

    void EndWrite()
    {
        AssertDebug(state == RenderProxyList::CS_WRITING);

        state = RenderProxyList::CS_WRITTEN;

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

        state = RenderProxyList::CS_READING;
    }

    void EndRead()
    {
        AssertDebug(state == RenderProxyList::CS_READING);
        state = RenderProxyList::CS_DONE;

        rwMarker.Decrement(2, MemoryOrder::RELEASE);
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

    Viewport viewport;
    int priority;

    ResourceTracker<ObjId<Entity>, RenderProxyMesh> meshes;
    ResourceTracker<ObjId<EnvProbe>, EnvProbe*> envProbes;
    ResourceTracker<ObjId<Light>, Light*> lights;
    ResourceTracker<ObjId<EnvGrid>, EnvGrid*> envGrids;
    ResourceTracker<ObjId<LightmapVolume>, LightmapVolume*> lightmapVolumes;
    ResourceTracker<ObjId<Material>, Material*> materials;
    ResourceTracker<ObjId<Texture>, Texture*> textures;
    ResourceTracker<ObjId<Skeleton>, Skeleton*> skeletons;

    ParallelRenderingState* parallelRenderingStateHead;
    ParallelRenderingState* parallelRenderingStateTail;

    FixedArray<FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>, RB_MAX> mappingsByBucket;

    // marker to set to locked when game thread is writing to this list.
    // this only really comes into play with non-buffered Views that do not double/triple buffer their RenderProxyLists
    AtomicVar<uint64> rwMarker { 0 };

#ifdef HYP_ENABLE_MT_CHECK
    HYP_DECLARE_MT_CHECK(dataRaceDetector);
#endif
};

class RenderCollector
{
public:
    static void CollectDrawCalls(RenderProxyList& renderProxyList, uint32 bucketBits);

    static void PerformOcclusionCulling(FrameBase* frame, const RenderSetup& renderSetup, RenderProxyList& renderProxyList, uint32 bucketBits);

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    static void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, RenderProxyList& renderProxyList, uint32 bucketBits);

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    static void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, RenderProxyList& renderProxyList, const FramebufferRef& framebuffer, uint32 bucketBits);
};

} // namespace hyperion
