/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Bricks/GlimmerBrickWorld.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickCVars.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerPass.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

#include <algorithm>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerBrickBuild("Rendering/GPU/Glimmer/BrickBuild");
static EngineStatGpuTimer s_statGlimmerBrickLight("Rendering/GPU/Glimmer/BrickLight");
static EngineStatGpuTimer s_statGlimmerBrickDebug("Rendering/GPU/Glimmer/BrickDebug");

static StaticShaderPropertyId s_propBrickModeInit { ShaderProperty(NAME("MODE"), NAME("INIT")) };
static StaticShaderPropertyId s_propBrickModeClear { ShaderProperty(NAME("MODE"), NAME("CLEAR")) };
static StaticShaderPropertyId s_propBrickModeBin { ShaderProperty(NAME("MODE"), NAME("BIN")) };
static StaticShaderPropertyId s_propBrickModeAlloc { ShaderProperty(NAME("MODE"), NAME("ALLOC")) };
static StaticShaderPropertyId s_propBrickModeCount { ShaderProperty(NAME("MODE"), NAME("COUNT")) };
static StaticShaderPropertyId s_propBrickModePrefix { ShaderProperty(NAME("MODE"), NAME("PREFIX")) };
static StaticShaderPropertyId s_propBrickModeGather { ShaderProperty(NAME("MODE"), NAME("GATHER")) };
static StaticShaderPropertyId s_propBrickModeTiles { ShaderProperty(NAME("MODE"), NAME("TILES")) };

static StaticShaderPropertyId s_propBrickLightModeLight { ShaderProperty(NAME("MODE"), NAME("LIGHT")) };
static StaticShaderPropertyId s_propBrickLightModeMips { ShaderProperty(NAME("MODE"), NAME("MIPS")) };

static constexpr uint32 BrickGroupSize = 64;
static constexpr uint32 BrickTableSize = GlimmerBrickGridXZ * GlimmerBrickGridY * GlimmerBrickGridXZ * 3;
static constexpr uint32 BrickListNodeCapacity = 1u << 20;
static constexpr uint32 BrickListNodeWords = 3;
// NOTE must match GlimmerBrickBuild.hlsl
static constexpr uint32 BrickNumCounters = 16;
static constexpr uint32 BrickCounterChunkFailedBricks = 8;
static constexpr uint32 BrickCounterChunkFailedPages = 9;
static constexpr uint32 BrickCounterChunkFailedCards = 10;
static constexpr uint32 BrickCounterChunkBricks = 11;
static constexpr uint32 BrickCounterChunkPages = 12;
static constexpr uint32 BrickCounterChunkCards = 13;
static constexpr uint32 BrickCountersBytes = BrickNumCounters * sizeof(uint32);
static constexpr uint32 BrickColumnsPerBrick = GlimmerBrickSide * GlimmerBrickSide;
static constexpr uint32 BrickTilesPerBrick = (GlimmerBrickSide / GlimmerBrickTile) * (GlimmerBrickSide / GlimmerBrickTile);
static constexpr uint32 BrickChunkBricks = GlimmerBrickChunkCells * GlimmerBrickChunkCells * GlimmerBrickChunkCells * 3;

static constexpr int32 WindowRecenterChunks = 2;
static constexpr uint32 MaxEmptyChunkClearsPerFrame = 32;
static constexpr uint32 PoolLogIntervalFrames = 300;
static constexpr float ChunkSize = GlimmerBrickCellSize * float(GlimmerBrickChunkCells);
static constexpr float ChangeMargin = 0.25f;
static constexpr float ShadowBiasColumns = 0.5f;
static constexpr float ShadowDistance = 500.0f;

static const Vec3i ChunkDims = Vec3i(int32(GlimmerBrickChunksXZ), int32(GlimmerBrickChunksY), int32(GlimmerBrickChunksXZ));

struct GlimmerBrickBuildConstants
{
    GlimmerBrickShaderData bricks;
    Vec4i boxMin;
    Vec4i boxExtent;
    Vec4u counts; // x = span chunks, y = groups along x, z = pool bricks, w = list node capacity
};

struct GlimmerBrickLightConstants
{
    GlimmerBrickShaderData bricks;
    Vec4i boxMin;
    Vec4i boxExtent;
    Vec4u range;  // x = first brick, y = bricks, z = 1 when they are the build list's, w = mip level
    Vec4f params; // x = columns off its surface a shadow ray starts, y = how far a shadow ray looks
    Vec4u dispatch; // x = groups along x
};

struct GlimmerBrickDebugConstants
{
    Vec4u dimensionsModeInstances;
    Vec4f params;
    Vec4u info;
    GlimmerBrickShaderData bricks;
};

#pragma region GlimmerBrickWorld

GlimmerBrickWorld::GlimmerBrickWorld()
    : m_poolBricks(0),
      m_poolPages(0),
      m_poolCards(0),
      m_relightCursor(0),
      m_seenGeneration(~0u),
      m_occupancyGeneration(~0u),
      m_numDirtyChunks(0),
      m_framesUnsettled(0),
      m_numEvictions(0),
      m_numStarved(0),
      m_framesSincePoolLog(0),
      m_hasWindow(false),
      m_shaderData {}
{
    const float columnWidth = GlimmerBrickCellSize / float(GlimmerBrickSide);

    m_shaderData.params = Vec4f(GlimmerBrickCellSize, 1.0f / GlimmerBrickCellSize, columnWidth, 1.0f / columnWidth);

    for (uint32& numPendingBuilds : m_numPendingBuilds)
    {
        numPendingBuilds = 0;
    }
}

GlimmerBrickWorld::~GlimmerBrickWorld()
{
    EnqueueDeletion(std::move(m_tableBuffer));
    EnqueueDeletion(std::move(m_indexBuffer));
    EnqueueDeletion(std::move(m_crossingsBuffer));
    EnqueueDeletion(std::move(m_materialBuffer));
    EnqueueDeletion(std::move(m_countersBuffer));
    EnqueueDeletion(std::move(m_freeListBuffer));
    EnqueueDeletion(std::move(m_pageFreeListBuffer));
    EnqueueDeletion(std::move(m_cardFreeListBuffer));
    EnqueueDeletion(std::move(m_radianceBuffer));
    EnqueueDeletion(std::move(m_crossingLightBuffer));
    EnqueueDeletion(std::move(m_listHeadsBuffer));
    EnqueueDeletion(std::move(m_listNodesBuffer));
    EnqueueDeletion(std::move(m_buildListBuffer));

    for (GpuBufferRef& readbackBuffer : m_readbackBuffers)
    {
        EnqueueDeletion(std::move(readbackBuffer));
    }
}

uint32 GlimmerBrickWorld::ChunkSlot(const Vec3i& coord)
{
    const uint32 x = uint32(coord.x) & (GlimmerBrickChunksXZ - 1);
    const uint32 y = uint32(coord.y) & (GlimmerBrickChunksY - 1);
    const uint32 z = uint32(coord.z) & (GlimmerBrickChunksXZ - 1);

    return (z * GlimmerBrickChunksY + y) * GlimmerBrickChunksXZ + x;
}

void GlimmerBrickWorld::CreateResources()
{
    m_poolBricks = uint32(MathUtil::Clamp(g_cvGlimmerBricksPoolBricks.Get(), 64, 16384));

    const auto createBuffer = [](size_t numWords, Name name)
    {
        GpuBufferRef buffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, numWords * sizeof(uint32), alignof(uint32));
        Check(buffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
        buffer->SetDebugName(name);
#endif

        return buffer;
    };

    m_poolPages = uint32(MathUtil::Clamp(g_cvGlimmerBricksPoolPages.Get(), 256, 65535));

    m_poolCards = uint32(MathUtil::Clamp(g_cvGlimmerBricksPoolCards.Get(), 64, 65535));

    const size_t numCrossings = size_t(m_poolPages) * GlimmerBrickPageCrossings;

    m_tableBuffer = createBuffer(BrickTableSize, NAME("GlimmerBrickTable"));
    m_indexBuffer = createBuffer(size_t(m_poolBricks) * GlimmerBrickIndexStride, NAME("GlimmerBrickIndex"));
    m_crossingsBuffer = createBuffer(numCrossings * 2, NAME("GlimmerBrickCrossings"));
    m_materialBuffer = createBuffer(numCrossings, NAME("GlimmerBrickMaterial"));
    m_crossingLightBuffer = createBuffer(numCrossings, NAME("GlimmerBrickCrossingLight"));
    m_countersBuffer = createBuffer(BrickNumCounters, NAME("GlimmerBrickCounters"));
    m_freeListBuffer = createBuffer(m_poolBricks, NAME("GlimmerBrickFreeList"));
    m_pageFreeListBuffer = createBuffer(m_poolPages, NAME("GlimmerBrickPageFreeList"));
    m_cardFreeListBuffer = createBuffer(m_poolCards, NAME("GlimmerBrickCardFreeList"));
    m_radianceBuffer = createBuffer(size_t(m_poolCards) * GlimmerBrickCardTexels, NAME("GlimmerBrickRadiance"));
    m_listHeadsBuffer = createBuffer(BrickChunkBricks, NAME("GlimmerBrickListHeads"));
    m_listNodesBuffer = createBuffer(size_t(BrickListNodeCapacity) * BrickListNodeWords, NAME("GlimmerBrickListNodes"));
    m_buildListBuffer = createBuffer(BrickChunkBricks, NAME("GlimmerBrickBuildList"));
}

void GlimmerBrickWorld::InsertBarriers(Frame* frame, bool isForWriting)
{
    CommandRecorder& cr = frame->cr;

    const ResourceState state = isForWriting ? ResourceState::UnorderedAccess : ResourceState::ShaderResource;

    cr << InsertBarrier(m_tableBuffer.Get(), state, ShaderModuleType::Compute);
    cr << InsertBarrier(m_indexBuffer.Get(), state, ShaderModuleType::Compute);
    cr << InsertBarrier(m_crossingsBuffer.Get(), state, ShaderModuleType::Compute);
    cr << InsertBarrier(m_materialBuffer.Get(), state, ShaderModuleType::Compute);
    cr << InsertBarrier(m_countersBuffer.Get(), state, ShaderModuleType::Compute);

    if (isForWriting)
    {
        cr << InsertBarrier(m_freeListBuffer.Get(), state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_pageFreeListBuffer.Get(), state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_cardFreeListBuffer.Get(), state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_listHeadsBuffer.Get(), state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_listNodesBuffer.Get(), state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_buildListBuffer.Get(), state, ShaderModuleType::Compute);
    }
}

void GlimmerBrickWorld::Dispatch(Frame* frame, BuildPass pass, const Box& box, uint32 numThreads, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache)
{
    CommandRecorder& cr = frame->cr;

    const Vec3u groups = RenderHelpers::WrapComputeGroupCount((numThreads + BrickGroupSize - 1) / BrickGroupSize);

    GlimmerBrickBuildConstants constants {};
    constants.bricks = m_shaderData;
    constants.boxMin = Vec4i(box.min.x, box.min.y, box.min.z, int32(m_poolCards));
    constants.boxExtent = Vec4i(box.extent.x, box.extent.y, box.extent.z, int32(m_poolPages));
    constants.counts = Vec4u(tlas.GetNumSpanChunks(), groups.x, m_poolBricks, BrickListNodeCapacity);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    const StaticShaderPropertyId* modeProperty = nullptr;

    switch (pass)
    {
    case BuildPass::Init:
        modeProperty = &s_propBrickModeInit;
        break;
    case BuildPass::Clear:
        modeProperty = &s_propBrickModeClear;
        break;
    case BuildPass::Bin:
        modeProperty = &s_propBrickModeBin;
        break;
    case BuildPass::Alloc:
        modeProperty = &s_propBrickModeAlloc;
        break;
    case BuildPass::Count:
        modeProperty = &s_propBrickModeCount;
        break;
    case BuildPass::Prefix:
        modeProperty = &s_propBrickModePrefix;
        break;
    case BuildPass::Gather:
        modeProperty = &s_propBrickModeGather;
        break;
    case BuildPass::Tiles:
        modeProperty = &s_propBrickModeTiles;
        break;
    }

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(*modeProperty);

    InsertBarriers(frame, true);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickBuild"), shaderProperties));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickTableBuffer"_sh, m_tableBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickIndexBuffer"_sh, m_indexBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingsBuffer"_sh, m_crossingsBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickMaterialBuffer"_sh, m_materialBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCountersBuffer"_sh, m_countersBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickFreeListBuffer"_sh, m_freeListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickPageFreeListBuffer"_sh, m_pageFreeListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCardFreeListBuffer"_sh, m_cardFreeListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickListHeadsBuffer"_sh, m_listHeadsBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickListNodesBuffer"_sh, m_listNodesBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickBuildListBuffer"_sh, m_buildListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "SpanInstancesBuffer"_sh, tlas.GetSpanInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanInstanceShaderData)));
    cr << SetShaderUniform(uniformIndex++, "SpanChunksBuffer"_sh, tlas.GetSpanChunksBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanChunkShaderData)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, blasCache.GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());

    cr << DispatchCompute(groups);
}

void GlimmerBrickWorld::MoveWindow(Frame* frame, const Vec3i& originChunk, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache)
{
    const Vec3i oldOrigin = m_originChunk;

    const bool overlapsOld = m_hasWindow
        && MathUtil::Abs(originChunk.x - oldOrigin.x) < ChunkDims.x
        && MathUtil::Abs(originChunk.y - oldOrigin.y) < ChunkDims.y
        && MathUtil::Abs(originChunk.z - oldOrigin.z) < ChunkDims.z;

    m_originChunk = originChunk;
    m_shaderData.origin = Vec4i(
        originChunk.x * int32(GlimmerBrickChunkCells),
        originChunk.y * int32(GlimmerBrickChunkCells),
        originChunk.z * int32(GlimmerBrickChunkCells),
        1);

    if (!overlapsOld)
    {
        Dispatch(frame, BuildPass::Init, Box {}, MathUtil::Max(MathUtil::Max(BrickTableSize, m_poolCards), MathUtil::Max(m_poolBricks, m_poolPages)), tlas, blasCache);
    }
    else
    {
        // the slabs entering the window share table slots with the ones leaving it
        Vec3i remainingMin = originChunk;
        Vec3i remainingMax = originChunk + ChunkDims;

        for (int axis = 0; axis < 3; axis++)
        {
            Vec3i enteringMin = remainingMin;
            Vec3i enteringMax = remainingMax;

            if (originChunk[axis] > oldOrigin[axis])
            {
                enteringMin[axis] = oldOrigin[axis] + ChunkDims[axis];
                remainingMax[axis] = enteringMin[axis];
            }
            else if (originChunk[axis] < oldOrigin[axis])
            {
                enteringMax[axis] = oldOrigin[axis];
                remainingMin[axis] = enteringMax[axis];
            }
            else
            {
                continue;
            }

            const int32 chunkCells = int32(GlimmerBrickChunkCells);

            Box box;
            box.min = Vec3i(enteringMin.x * chunkCells, enteringMin.y * chunkCells, enteringMin.z * chunkCells);
            box.extent = Vec3i((enteringMax.x - enteringMin.x) * chunkCells, (enteringMax.y - enteringMin.y) * chunkCells, (enteringMax.z - enteringMin.z) * chunkCells);

            Dispatch(frame, BuildPass::Clear, box, uint32(box.extent.x * box.extent.y * box.extent.z) * 3, tlas, blasCache);
        }
    }

    for (int32 z = 0; z < ChunkDims.z; z++)
    {
        for (int32 y = 0; y < ChunkDims.y; y++)
        {
            for (int32 x = 0; x < ChunkDims.x; x++)
            {
                const Vec3i coord = originChunk + Vec3i(x, y, z);

                Chunk& chunk = m_chunks[ChunkSlot(coord)];

                // what was starved may fit now that a slab has left
                chunk.isStarved = false;

                if (overlapsOld && chunk.coord == coord)
                {
                    continue;
                }

                chunk.coord = coord;
                chunk.isBuilt = false;
                chunk.hasBricks = false;
                chunk.isDirty = true;
                chunk.buildSerial++;
                chunk.numBricks = 0;
                chunk.numPages = 0;
                chunk.numCards = 0;
            }
        }
    }

    m_hasWindow = true;
}

void GlimmerBrickWorld::MarkSceneChanges(const GlimmerTLAS& tlas)
{
    const uint32 generation = tlas.GetGeneration();

    if (generation == m_seenGeneration)
    {
        return;
    }

    GlimmerSceneChanges changes;

    const bool isEverywhere = m_seenGeneration == ~0u || !tlas.GetChangesSince(m_seenGeneration, changes);
    const bool skipLodOnlyChanges = g_cvGlimmerSkipLodOnlyChanges.Get();

    m_seenGeneration = generation;

    for (Chunk& chunk : m_chunks)
    {
        if (isEverywhere)
        {
            chunk.isDirty = true;
            chunk.isStarved = false;

            continue;
        }

        const Vec3f chunkMin = Vec3f(float(chunk.coord.x), float(chunk.coord.y), float(chunk.coord.z)) * ChunkSize - Vec3f(ChangeMargin);
        const Vec3f chunkMax = chunkMin + Vec3f(ChunkSize + 2.0f * ChangeMargin);

        for (const GlimmerSceneChange& change : changes)
        {
            if ((chunk.isDirty && !chunk.isStarved) || (change.isLodOnly && skipLodOnlyChanges))
            {
                continue;
            }

            if (change.bounds.Overlaps(BoundingBox(chunkMin, chunkMax)))
            {
                chunk.isDirty = true;
                chunk.isStarved = false;
            }
        }
    }
}

void GlimmerBrickWorld::MarkOccupiedChunks(const GlimmerTLAS& tlas)
{
    for (Chunk& chunk : m_chunks)
    {
        chunk.hasGeometry = false;
    }

    const Vec3i windowMax = m_originChunk + ChunkDims - Vec3i(1);

    for (const GlimmerTLAS::SpanKey& spanKey : tlas.GetSpanKeys())
    {
        const Vec3f boundsMin = (spanKey.bounds.min - Vec3f(ChangeMargin)) / ChunkSize;
        const Vec3f boundsMax = (spanKey.bounds.max + Vec3f(ChangeMargin)) / ChunkSize;

        Vec3i chunkMin;
        Vec3i chunkMax;

        for (int axis = 0; axis < 3; axis++)
        {
            chunkMin[axis] = MathUtil::Max(int32(MathUtil::Floor(boundsMin[axis])), m_originChunk[axis]);
            chunkMax[axis] = MathUtil::Min(int32(MathUtil::Floor(boundsMax[axis])), windowMax[axis]);
        }

        for (int32 z = chunkMin.z; z <= chunkMax.z; z++)
        {
            for (int32 y = chunkMin.y; y <= chunkMax.y; y++)
            {
                for (int32 x = chunkMin.x; x <= chunkMax.x; x++)
                {
                    m_chunks[ChunkSlot(Vec3i(x, y, z))].hasGeometry = true;
                }
            }
        }
    }
}

void GlimmerBrickWorld::BuildChunk(Frame* frame, Chunk& chunk, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache, const GlimmerTechnique* technique)
{
    const int32 chunkCells = int32(GlimmerBrickChunkCells);

    Box box;
    box.min = Vec3i(chunk.coord.x * chunkCells, chunk.coord.y * chunkCells, chunk.coord.z * chunkCells);
    box.extent = Vec3i(chunkCells, chunkCells, chunkCells);

    Dispatch(frame, BuildPass::Clear, box, BrickChunkBricks, tlas, blasCache);

    if (tlas.GetNumSpanChunks() != 0)
    {
        // a group per span chunk
        Dispatch(frame, BuildPass::Bin, box, tlas.GetNumSpanChunks() * BrickGroupSize, tlas, blasCache);
        Dispatch(frame, BuildPass::Alloc, box, BrickChunkBricks, tlas, blasCache);
        Dispatch(frame, BuildPass::Count, box, BrickChunkBricks * BrickColumnsPerBrick, tlas, blasCache);
        Dispatch(frame, BuildPass::Prefix, box, BrickChunkBricks, tlas, blasCache);
        Dispatch(frame, BuildPass::Gather, box, BrickChunkBricks * BrickColumnsPerBrick, tlas, blasCache);
        Dispatch(frame, BuildPass::Tiles, box, BrickChunkBricks * BrickTilesPerBrick, tlas, blasCache);

        LightBricks(frame, technique, 0, BrickChunkBricks, &box);
    }

    chunk.isBuilt = true;
    chunk.isDirty = false;
    chunk.hasBricks = true;
    chunk.buildSerial++;

    CopyBuildCounters(frame, chunk);
}

void GlimmerBrickWorld::ClearChunk(Frame* frame, Chunk& chunk, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache)
{
    const int32 chunkCells = int32(GlimmerBrickChunkCells);

    Box box;
    box.min = Vec3i(chunk.coord.x * chunkCells, chunk.coord.y * chunkCells, chunk.coord.z * chunkCells);
    box.extent = Vec3i(chunkCells, chunkCells, chunkCells);

    Dispatch(frame, BuildPass::Clear, box, BrickChunkBricks, tlas, blasCache);

    chunk.hasBricks = false;
    chunk.numBricks = 0;
    chunk.numPages = 0;
    chunk.numCards = 0;
}

void GlimmerBrickWorld::CopyBuildCounters(Frame* frame, const Chunk& chunk)
{
    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;

    uint32& numPending = m_numPendingBuilds[slot];

    if (numPending == GlimmerBrickMaxChunkBuildsPerFrame)
    {
        return;
    }

    if (!m_readbackBuffers[slot].IsValid())
    {
        m_readbackBuffers[slot] = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, GlimmerBrickMaxChunkBuildsPerFrame * BrickCountersBytes);
        m_readbackBuffers[slot]->SetIsCpuAccessible(true);
        Check(m_readbackBuffers[slot]->Create());
    }

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_countersBuffer.Get(), ResourceState::CopySrc, ShaderModuleType::Compute);
    cr << InsertBarrier(m_readbackBuffers[slot].Get(), ResourceState::CopyDst, ShaderModuleType::Compute);
    cr << CopyBuffer(m_countersBuffer.Get(), m_readbackBuffers[slot].Get(), 0, numPending * BrickCountersBytes, BrickCountersBytes);
    cr << InsertBarrier(m_countersBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    m_pendingBuilds[slot][numPending++] = ChunkBuild { chunk.coord, chunk.buildSerial };
}

bool GlimmerBrickWorld::ReadBuildCounters(Frame* frame, const Vec3f& viewerPosition, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache)
{
    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;
    const uint32 numPending = m_numPendingBuilds[slot];

    m_numPendingBuilds[slot] = 0;

    // this frame slot's last submission has finished by now, so what it copied can be read
    if (numPending == 0)
    {
        return false;
    }

    FixedArray<uint32, GlimmerBrickMaxChunkBuildsPerFrame * BrickNumCounters> counters;

    m_readbackBuffers[slot]->Invalidate();
    m_readbackBuffers[slot]->Read(numPending * BrickCountersBytes, counters.Data());

    bool hasWritten = false;

    for (uint32 buildIndex = 0; buildIndex < numPending; buildIndex++)
    {
        const ChunkBuild& build = m_pendingBuilds[slot][buildIndex];
        Chunk& chunk = m_chunks[ChunkSlot(build.coord)];

        // built again, cleared or gone from the window since
        if (chunk.coord != build.coord || chunk.buildSerial != build.buildSerial || chunk.isDirty)
        {
            continue;
        }

        const uint32* record = counters.Data() + size_t(buildIndex) * BrickNumCounters;

        chunk.numBricks = record[BrickCounterChunkBricks];
        chunk.numPages = record[BrickCounterChunkPages];
        chunk.numCards = record[BrickCounterChunkCards];

        const Vec3u needed = Vec3u(record[BrickCounterChunkFailedBricks], record[BrickCounterChunkFailedPages], record[BrickCounterChunkFailedCards]);

        if (needed.x == 0 && needed.y == 0 && needed.z == 0)
        {
            continue;
        }

        // it keeps what it got until it's built again
        chunk.isDirty = true;

        if (MakeRoom(frame, chunk, viewerPosition, needed, tlas, blasCache))
        {
            hasWritten = true;
        }
        else
        {
            chunk.isStarved = true;
            m_numStarved++;
        }
    }

    return hasWritten;
}

static float ChunkDistance(const Vec3i& coord, const Vec3f& viewerPosition)
{
    const Vec3f center = (Vec3f(float(coord.x), float(coord.y), float(coord.z)) + Vec3f(0.5f)) * ChunkSize;

    return center.Distance(viewerPosition);
}

bool GlimmerBrickWorld::MakeRoom(Frame* frame, const Chunk& requester, const Vec3f& viewerPosition, const Vec3u& needed, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache)
{
    struct Candidate
    {
        Chunk* chunk;
        float distance;
    };

    // only from chunks a chunk farther, so two at about the same distance don't take turns
    const float minDistance = ChunkDistance(requester.coord, viewerPosition) + ChunkSize;

    Array<Candidate, GlimmerTempAllocator> candidates;
    candidates.Reserve(m_chunks.Size());

    for (Chunk& chunk : m_chunks)
    {
        if (!chunk.hasBricks || (chunk.numBricks == 0 && chunk.numPages == 0 && chunk.numCards == 0))
        {
            continue;
        }

        const float distance = ChunkDistance(chunk.coord, viewerPosition);

        if (distance > minDistance)
        {
            candidates.PushBack(Candidate { &chunk, distance });
        }
    }

    std::sort(candidates.Begin(), candidates.End(), [](const Candidate& lhs, const Candidate& rhs)
        {
            return lhs.distance > rhs.distance;
        });

    Vec3u freed = Vec3u::Zero();
    size_t numVictims = 0;

    const auto isEnough = [&]()
    {
        return freed.x >= needed.x && freed.y >= needed.y && freed.z >= needed.z;
    };

    while (numVictims < candidates.Size() && !isEnough())
    {
        const Chunk& victim = *candidates[numVictims++].chunk;

        freed += Vec3u(victim.numBricks, victim.numPages, victim.numCards);
    }

    if (!isEnough())
    {
        return false;
    }

    // given up until the window moves, rather than built again to push out the next one along
    for (size_t victimIndex = 0; victimIndex < numVictims; victimIndex++)
    {
        Chunk& victim = *candidates[victimIndex].chunk;

        ClearChunk(frame, victim, tlas, blasCache);

        victim.isDirty = true;
        victim.isStarved = true;
    }

    m_numEvictions += uint32(numVictims);

    return true;
}

void GlimmerBrickWorld::Update(Frame* frame, const Vec3f& viewerPosition, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache, const GlimmerTechnique* technique)
{
    HYP_SCOPE;

    if (!tlas.IsReady() || !blasCache.IsReady() || !tlas.GetSpanInstancesBuffer().IsValid() || !tlas.GetSpanChunksBuffer().IsValid())
    {
        return;
    }

    if (!m_tableBuffer.IsValid())
    {
        CreateResources();
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickBuild);

    const Vec3i viewerChunk = Vec3i(
        int32(MathUtil::Floor(viewerPosition.x / ChunkSize)),
        int32(MathUtil::Floor(viewerPosition.y / ChunkSize)),
        int32(MathUtil::Floor(viewerPosition.z / ChunkSize)));

    const Vec3i originChunk = viewerChunk - Vec3i(ChunkDims.x / 2, ChunkDims.y / 2, ChunkDims.z / 2);

    bool hasWritten = false;

    if (!m_hasWindow
        || MathUtil::Abs(originChunk.x - m_originChunk.x) >= WindowRecenterChunks
        || MathUtil::Abs(originChunk.y - m_originChunk.y) >= WindowRecenterChunks
        || MathUtil::Abs(originChunk.z - m_originChunk.z) >= WindowRecenterChunks)
    {
        MoveWindow(frame, originChunk, tlas, blasCache);

        m_occupancyGeneration = ~0u;

        hasWritten = true;
    }

    MarkSceneChanges(tlas);

    if (m_occupancyGeneration != tlas.GetGeneration())
    {
        MarkOccupiedChunks(tlas);

        m_occupancyGeneration = tlas.GetGeneration();
    }

    // a chunk that ran out of room gets it from chunks farther away, or waits
    if (ReadBuildCounters(frame, viewerPosition, tlas, blasCache))
    {
        hasWritten = true;
    }

    // a chunk no span instance reaches is done without a build, and only cleared when it had bricks
    uint32 numClears = 0;

    for (Chunk& chunk : m_chunks)
    {
        if (!chunk.isDirty || chunk.hasGeometry)
        {
            continue;
        }

        if (chunk.hasBricks)
        {
            if (numClears == MaxEmptyChunkClearsPerFrame)
            {
                continue;
            }

            ClearChunk(frame, chunk, tlas, blasCache);

            numClears++;
            hasWritten = true;
        }

        chunk.isBuilt = true;
        chunk.isDirty = false;
        chunk.isStarved = false;
    }

    const int32 numBuilds = MathUtil::Clamp(g_cvGlimmerBricksChunksPerFrame.Get(), 1, int32(GlimmerBrickMaxChunkBuildsPerFrame));

    for (int32 buildIndex = 0; buildIndex < numBuilds; buildIndex++)
    {
        Chunk* nearestChunk = nullptr;
        float nearestDistance = MathUtil::MaxSafeValue<float>();

        for (Chunk& chunk : m_chunks)
        {
            if (!chunk.isDirty || !chunk.hasGeometry || chunk.isStarved)
            {
                continue;
            }

            const Vec3f center = (Vec3f(float(chunk.coord.x), float(chunk.coord.y), float(chunk.coord.z)) + Vec3f(0.5f)) * ChunkSize;
            const float distance = center.DistanceSquared(viewerPosition);

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestChunk = &chunk;
            }
        }

        if (!nearestChunk)
        {
            break;
        }

        BuildChunk(frame, *nearestChunk, tlas, blasCache, technique);

        hasWritten = true;
    }

    const bool wasSettled = IsSettled();

    m_numDirtyChunks = 0;

    for (const Chunk& chunk : m_chunks)
    {
        m_numDirtyChunks += chunk.isDirty && !chunk.isStarved ? 1 : 0;
    }

    if (IsSettled() && !wasSettled)
    {
        HYP_LOG(Rendering, Info, "Glimmer: bricks settled after {} frames", m_framesUnsettled);
    }

    m_framesUnsettled = IsSettled() ? 0 : m_framesUnsettled + 1;

    m_framesSincePoolLog++;

    if ((m_numEvictions != 0 || m_numStarved != 0) && m_framesSincePoolLog >= PoolLogIntervalFrames)
    {
        HYP_LOG(Rendering, Warning, "Glimmer: brick pools full; {} chunks given up for nearer ones, {} left without room", m_numEvictions, m_numStarved);

        m_numEvictions = 0;
        m_numStarved = 0;
        m_framesSincePoolLog = 0;
    }

    if (hasWritten)
    {
        InsertBarriers(frame, false);
    }

    const uint32 numRelightBricks = MathUtil::Min(uint32(MathUtil::Clamp(g_cvGlimmerBricksRelightBricks.Get(), 0, 4096)), m_poolBricks);

    if (numRelightBricks != 0)
    {
        const uint32 firstBrick = MathUtil::Min(m_relightCursor, m_poolBricks - numRelightBricks);

        LightBricks(frame, technique, firstBrick, numRelightBricks, nullptr);

        m_relightCursor = (firstBrick + numRelightBricks) % m_poolBricks;
    }
}

void GlimmerBrickWorld::LightBricks(Frame* frame, const GlimmerTechnique* technique, uint32 firstBrick, uint32 numBricks, const Box* buildListBox)
{
    HYP_SCOPE;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickLight);

    CommandRecorder& cr = frame->cr;

    InsertBarriers(frame, false);

    cr << InsertBarrier(m_buildListBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

    const bool hasIndirect = technique && technique->IsReady();

    const auto dispatchPass = [&](const StaticShaderPropertyId& modeProperty, uint32 level, uint32 numThreads)
    {
        const Vec3u groups = RenderHelpers::WrapComputeGroupCount((numThreads + BrickGroupSize - 1) / BrickGroupSize);

        GlimmerApplyShaderData applyShaderData {};
        applyShaderData.params = Vec4u(0u, hasIndirect ? 1u : 0u, 0u, 0u);
        applyShaderData.settings = Vec4f(MathUtil::Max(g_cvGlimmerIntensity.Get(), 0.0f), 0.0f, 0.0f, 0.0f);

        GlimmerBrickLightConstants constants {};
        constants.bricks = m_shaderData;
        constants.range = Vec4u(firstBrick, numBricks, buildListBox ? 1u : 0u, level);
        constants.params = Vec4f(ShadowBiasColumns, ShadowDistance, 0.0f, 0.0f);
        constants.dispatch = Vec4u(groups.x, 0, 0, 0);

        if (buildListBox)
        {
            constants.boxMin = Vec4i(buildListBox->min.x, buildListBox->min.y, buildListBox->min.z, 0);
            constants.boxExtent = Vec4i(buildListBox->extent.x, buildListBox->extent.y, buildListBox->extent.z, 0);
        }

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&applyShaderData);
        GlimmerTechnique::WriteApplyShaderData(*RI.cbufferAllocator, hasIndirect ? technique : nullptr);
        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);

        cr << InsertBarrier(m_radianceBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_crossingLightBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickLight"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickTableBuffer"_sh, m_tableBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickIndexBuffer"_sh, m_indexBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingsBuffer"_sh, m_crossingsBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickMaterialBuffer"_sh, m_materialBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCountersBuffer"_sh, m_countersBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickBuildListBuffer"_sh, m_buildListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickRadianceBuffer"_sh, m_radianceBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingLightBuffer"_sh, m_crossingLightBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));

        uniformIndex = GlimmerTechnique::BindApplyResources(cr, uniformIndex, hasIndirect ? technique : nullptr);

        cr << DispatchCompute(groups);
    };

    dispatchPass(s_propBrickLightModeLight, 0, numBricks * BrickColumnsPerBrick);

    for (uint32 level = GlimmerBrickCardFirstMip; level < GlimmerBrickMips; level++)
    {
        const uint32 side = GlimmerBrickSide >> level;

        dispatchPass(s_propBrickLightModeMips, level, numBricks * GlimmerBrickCards * side * side);
    }

    cr << InsertBarrier(m_radianceBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_crossingLightBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
}

void GlimmerBrickWorld::GetBuildingChunks(Vec4i& outOrigin, Vec4u (&outBits)[16]) const
{
    for (Vec4u& bits : outBits)
    {
        bits = Vec4u::Zero();
    }

    if (!IsReady())
    {
        outOrigin = Vec4i(0, 0, 0, 2);

        return;
    }

    outOrigin = Vec4i(m_originChunk.x, m_originChunk.y, m_originChunk.z, 1);

    for (const Chunk& chunk : m_chunks)
    {
        if ((chunk.isBuilt && !chunk.isDirty) || chunk.isStarved)
        {
            continue;
        }

        const Vec3i local = chunk.coord - m_originChunk;

        if (local.x < 0 || local.y < 0 || local.z < 0 || local.x >= ChunkDims.x || local.y >= ChunkDims.y || local.z >= ChunkDims.z)
        {
            continue;
        }

        const uint32 bit = uint32((local.z * ChunkDims.y + local.y) * ChunkDims.x + local.x);

        outBits[bit >> 7][(bit >> 5) & 3] |= 1u << (bit & 31);
    }
}

bool GlimmerBrickWorld::RenderDebugView(const GlimmerDebugViewContext& context, GlimmerBrickDebugView debugView)
{
    HYP_SCOPE;

    if (!IsReady() || !context.tlas || !context.tlas->IsReady() || !context.blasCache || !context.blasCache->IsReady())
    {
        return false;
    }

    const GlimmerTLAS& tlas = *context.tlas;
    const GlimmerBLASCache& blasCache = *context.blasCache;

    Framebuffer* gbufferFramebuffer = context.gbufferFramebuffer;
    const Vec2u extent = context.extent;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickDebug);

    GlimmerBrickDebugConstants constants {};
    constants.dimensionsModeInstances = Vec4u(extent.x, extent.y, uint32(debugView), tlas.GetNumInstances());
    constants.params = Vec4f(10000.0f, 0.0f, 0.0f, 0.0f);
    constants.info = Vec4u(m_poolBricks, m_poolPages, m_poolCards, 0);
    constants.bricks = m_shaderData;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = context.frame->cr;

    cr << InsertBarrier(gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);

    InsertBarriers(context.frame, false);
    cr << InsertBarrier(m_radianceBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_crossingLightBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickDebug")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "OutImage"_sh, context.outputImageView);
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(context.view->GetCamera()));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickTableBuffer"_sh, m_tableBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickIndexBuffer"_sh, m_indexBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingsBuffer"_sh, m_crossingsBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickMaterialBuffer"_sh, m_materialBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCountersBuffer"_sh, m_countersBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickRadianceBuffer"_sh, m_radianceBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingLightBuffer"_sh, m_crossingLightBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });

    return true;
}

#pragma endregion GlimmerBrickWorld

} // namespace Hyperion
