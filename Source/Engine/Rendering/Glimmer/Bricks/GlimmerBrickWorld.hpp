/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

/////////////////////////////////////////////////////////////////////////////////////////////
/// Glimmer's "Bricks" system is loosely based on Layered Depth Images (Shade et all, 98) ///
///   https://www.cs.princeton.edu/courses/archive/spring2001/cs598b/papers/shade98.pdf   ///
/////////////////////////////////////////////////////////////////////////////////////////////

class GlimmerSceneInstances;
class GlimmerTriangleCache;
class GlimmerTechnique;
struct GlimmerDebugViewContext;

static constexpr uint32 GlimmerBrickGridXZ = 128;
static constexpr uint32 GlimmerBrickGridY = 64;
static constexpr uint32 GlimmerBrickChunkCells = 8;
static constexpr uint32 GlimmerBrickSide = 32;
static constexpr uint32 GlimmerBrickTile = 4;
static constexpr uint32 GlimmerBrickPageCrossings = 256;
static constexpr uint32 GlimmerBrickMaxPages = 31;
static constexpr uint32 GlimmerBrickCards = 8;
static constexpr uint32 GlimmerBrickMips = 6;
static constexpr uint32 GlimmerBrickCardFirstMip = 2;
static constexpr uint32 GlimmerBrickCardSide = GlimmerBrickSide >> GlimmerBrickCardFirstMip;
static constexpr uint32 GlimmerBrickCardTexels = (4 * GlimmerBrickCardSide * GlimmerBrickCardSide - 1) / 3;
static constexpr uint32 GlimmerBrickIndexStride = 1 + GlimmerBrickMaxPages + GlimmerBrickCards + 6 + (GlimmerBrickSide / GlimmerBrickTile) * (GlimmerBrickSide / GlimmerBrickTile) + GlimmerBrickSide * GlimmerBrickSide;

static constexpr uint32 GlimmerBrickMaxPoolBricks = 32768;
static constexpr uint32 GlimmerBrickMaxPoolPages = 65535;
static constexpr uint32 GlimmerBrickMaxPoolCards = 65535;

// NOTE must match GlimmerBricks.hlsli: a reflection slot keeps its card in this many bits and the brick's serial above them
static constexpr uint32 GlimmerBrickCardBits = 18;

static_assert(GlimmerBrickMaxPoolBricks * GlimmerBrickCards <= (1u << GlimmerBrickCardBits));

static constexpr float GlimmerBrickCellSize = 2.0f;

static constexpr uint32 GlimmerBrickMaxChunkBuildsPerFrame = 64;

static constexpr uint32 GlimmerBrickChunksXZ = GlimmerBrickGridXZ / GlimmerBrickChunkCells;
static constexpr uint32 GlimmerBrickChunksY = GlimmerBrickGridY / GlimmerBrickChunkCells;
static constexpr uint32 GlimmerBrickChunks = GlimmerBrickChunksXZ * GlimmerBrickChunksY * GlimmerBrickChunksXZ;

struct GlimmerBrickShaderData
{
    Vec4i origin; // xyz = absolute cell of the window's first cell, w = 1 once the window is valid
    Vec4f params; // x = cell size, y = 1 / cell size, z = column width, w = 1 / column width
};

static_assert(sizeof(GlimmerBrickShaderData) == 32);

enum class GlimmerBrickDebugView : int
{
    None = 0,
    Shaded,
    Steps,
    Receiver,
    Light,
    LightCoarse,

    Max
};

class GlimmerBrickWorld final
{
public:
    GlimmerBrickWorld();

    GlimmerBrickWorld(const GlimmerBrickWorld& other) = delete;
    GlimmerBrickWorld& operator=(const GlimmerBrickWorld& other) = delete;

    ~GlimmerBrickWorld();

    // technique: where the cards' indirect light comes from
    void Update(Frame* frame, const Vec3f& viewerPosition, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache, const GlimmerTechnique* technique);

    bool RenderDebugView(const GlimmerDebugViewContext& context, GlimmerBrickDebugView debugView);

    // a bit per chunk of the window, by its place in it, set while it waits to be built (not while starved); origin.w is 0 without bricks, 1 with, 2 before any are built
    void GetBuildingChunks(Vec4i& outOrigin, Vec4u (&outBits)[16]) const;

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_tableBuffer.IsValid() && m_shaderData.origin.w != 0;
    }

    // every chunk of the window built for the scene as it is
    HYP_FORCE_INLINE bool IsSettled() const
    {
        return IsReady() && m_numDirtyChunks == 0;
    }

    HYP_FORCE_INLINE const GlimmerBrickShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetTableBuffer() const
    {
        return m_tableBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetIndexBuffer() const
    {
        return m_indexBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetCrossingsBuffer() const
    {
        return m_crossingsBuffer;
    }

    HYP_FORCE_INLINE uint32 GetNumCrossings() const
    {
        return m_poolPages * GlimmerBrickPageCrossings;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetRadianceBuffer() const
    {
        return m_radianceBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetCrossingLightBuffer() const
    {
        return m_crossingLightBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetMaterialBuffer() const
    {
        return m_materialBuffer;
    }

private:
    struct Chunk
    {
        Vec3i coord;
        bool isBuilt = false;
        bool isDirty = false;
        bool hasGeometry = false;
        bool hasBricks = false;
        bool isStarved = false; // failed for want of room nothing farther could give up; tried again once the window moves
        bool isShort = false;
        uint32 buildSerial = 0;
        uint32 numBricks = 0;
        uint32 numPages = 0;
        uint32 numCards = 0;
    };

    struct ChunkBuild
    {
        Vec3i coord;
        uint32 buildSerial;
    };

    struct Box
    {
        Vec3i min;
        Vec3i extent;
    };

    enum class BuildPass : uint8
    {
        Init,
        Clear,
        Bin,
        Alloc,
        Count,
        Prefix,
        Gather,
        Tiles,
        Live,
        Grow
    };

    void CreateResources();
    void CreatePoolResources(uint32 poolBricks, uint32 poolPages, uint32 poolCards);
    void GrowPoolResources(Frame* frame, const Vec3u& grown, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache);
    bool GrowPools(const Vec3u& needed);
    void ListLiveBricks(Frame* frame, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache);
    void MoveWindow(Frame* frame, const Vec3i& originChunk, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache);
    void MarkSceneChanges(const GlimmerSceneInstances& sceneInstances);
    void MarkOccupiedChunks(const GlimmerSceneInstances& sceneInstances);
    void BuildChunk(Frame* frame, Chunk& chunk, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache, const GlimmerTechnique* technique);
    void LightBricks(Frame* frame, const GlimmerTechnique* technique, uint32 firstBrick, uint32 numBricks, const Box* buildListBox);
    void Dispatch(Frame* frame, BuildPass pass, const Box& box, uint32 numThreads, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache, uint32 step = 0);
    void InsertBarriers(Frame* frame, bool isForWriting);
    void CopyBuildCounters(Frame* frame, const Chunk& chunk);
    bool ReadBuildCounters(Frame* frame, const Vec3f& viewerPosition, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache);
    bool MakeRoom(Frame* frame, const Chunk& requester, const Vec3f& viewerPosition, const Vec3u& needed, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache);
    void ClearChunk(Frame* frame, Chunk& chunk, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache);

    static uint32 ChunkSlot(const Vec3i& coord);

    GpuBufferRef m_tableBuffer;
    GpuBufferRef m_indexBuffer;
    GpuBufferRef m_crossingsBuffer;
    GpuBufferRef m_materialBuffer;
    GpuBufferRef m_countersBuffer;
    GpuBufferRef m_freeListBuffer;
    GpuBufferRef m_pageFreeListBuffer;
    GpuBufferRef m_cardFreeListBuffer;
    GpuBufferRef m_radianceBuffer;
    GpuBufferRef m_crossingLightBuffer;
    GpuBufferRef m_listNodesBuffer;
    GpuBufferRef m_liveListBuffer;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_readbackBuffers;
    FixedArray<FixedArray<ChunkBuild, GlimmerBrickMaxChunkBuildsPerFrame>, NumFramesInFlight> m_pendingBuilds;
    FixedArray<uint32, NumFramesInFlight> m_numPendingBuilds;

    FixedArray<Chunk, GlimmerBrickChunks> m_chunks;

    Vec3i m_originChunk;
    uint32 m_poolBricks;
    uint32 m_poolPages;
    uint32 m_poolCards;
    uint32 m_relightCursor;
    uint32 m_seenGeneration;
    uint32 m_occupancyGeneration;
    uint32 m_numDirtyChunks;
    uint32 m_framesUnsettled;
    uint32 m_numEvictions;
    uint32 m_numStarved;
    uint32 m_framesSincePoolLog;
    uint32 m_framesSinceLightView;
    Vec3u m_grownPools;
    bool m_hasWindow;
    bool m_isTextured;

    GlimmerBrickShaderData m_shaderData;
};

} // namespace Hyperion
