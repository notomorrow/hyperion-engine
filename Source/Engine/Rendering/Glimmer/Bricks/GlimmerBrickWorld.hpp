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

class GlimmerTLAS;
class GlimmerBLASCache;
struct GlimmerDebugViewContext;

// NOTE must match GlimmerBricks.hlsli
static constexpr uint32 GlimmerBrickGridXZ = 64;
static constexpr uint32 GlimmerBrickGridY = 32;
static constexpr uint32 GlimmerBrickChunkCells = 8;
static constexpr uint32 GlimmerBrickSide = 32;
static constexpr uint32 GlimmerBrickTile = 4;
static constexpr uint32 GlimmerBrickPageCrossings = 256;
static constexpr uint32 GlimmerBrickMaxPages = 31;
static constexpr uint32 GlimmerBrickIndexStride = 1 + GlimmerBrickMaxPages + (GlimmerBrickSide / GlimmerBrickTile) * (GlimmerBrickSide / GlimmerBrickTile) + GlimmerBrickSide * GlimmerBrickSide;

static constexpr float GlimmerBrickCellSize = 2.0f;

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
    Error, // against the BVH: green within a column, yellow within four, red a leak, blue a phantom
    Receiver,

    Max
};

class GlimmerBrickWorld final
{
public:
    GlimmerBrickWorld();

    GlimmerBrickWorld(const GlimmerBrickWorld& other) = delete;
    GlimmerBrickWorld& operator=(const GlimmerBrickWorld& other) = delete;

    ~GlimmerBrickWorld();

    void Update(Frame* frame, const Vec3f& viewerPosition, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache);

    bool RenderDebugView(const GlimmerDebugViewContext& context, GlimmerBrickDebugView debugView);

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_tableBuffer.IsValid() && m_shaderData.origin.w != 0;
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

    HYP_FORCE_INLINE const GpuBufferRef& GetExtraBuffer() const
    {
        return m_extraBuffer;
    }

private:
    struct Chunk
    {
        Vec3i coord;
        bool isBuilt = false;
        bool isDirty = false;
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
        Tiles
    };

    void CreateResources();
    void MoveWindow(Frame* frame, const Vec3i& originChunk, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache);
    void MarkSceneChanges(const GlimmerTLAS& tlas);
    void BuildChunk(Frame* frame, Chunk& chunk, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache);
    void Dispatch(Frame* frame, BuildPass pass, const Box& box, uint32 numThreads, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache);
    void InsertBarriers(Frame* frame, bool isForWriting);

    static uint32 ChunkSlot(const Vec3i& coord);

    GpuBufferRef m_tableBuffer;
    GpuBufferRef m_indexBuffer;
    GpuBufferRef m_crossingsBuffer;
    GpuBufferRef m_extraBuffer;
    GpuBufferRef m_countersBuffer;
    GpuBufferRef m_freeListBuffer;
    GpuBufferRef m_pageFreeListBuffer;
    GpuBufferRef m_listHeadsBuffer;
    GpuBufferRef m_listNodesBuffer;
    GpuBufferRef m_buildListBuffer;

    FixedArray<Chunk, GlimmerBrickChunks> m_chunks;

    Vec3i m_originChunk;
    uint32 m_poolBricks;
    uint32 m_poolPages;
    uint32 m_seenGeneration;
    bool m_hasWindow;

    GlimmerBrickShaderData m_shaderData;
};

} // namespace Hyperion
