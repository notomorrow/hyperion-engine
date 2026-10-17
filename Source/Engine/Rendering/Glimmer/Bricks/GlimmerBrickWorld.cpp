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
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerGroundGI.hpp>
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
#include <iterator>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerBrickBuild("Rendering/GPU/Glimmer/BrickBuild");
static EngineStatGpuTimer s_statGlimmerBrickLight("Rendering/GPU/Glimmer/BrickLight");
static EngineStatGpuTimer s_statGlimmerBrickDebug("Rendering/GPU/Glimmer/BrickDebug");
static EngineStatGpuTimer s_statGlimmerBrickGITrace("Rendering/GPU/Glimmer/BrickGITrace");
static EngineStatGpuTimer s_statGlimmerBrickPassClear("Rendering/GPU/Glimmer/BrickPasses/Clear");
static EngineStatGpuTimer s_statGlimmerBrickPassBin("Rendering/GPU/Glimmer/BrickPasses/Bin");
static EngineStatGpuTimer s_statGlimmerBrickPassAlloc("Rendering/GPU/Glimmer/BrickPasses/Alloc");
static EngineStatGpuTimer s_statGlimmerBrickPassCount("Rendering/GPU/Glimmer/BrickPasses/Count");
static EngineStatGpuTimer s_statGlimmerBrickPassPrefix("Rendering/GPU/Glimmer/BrickPasses/Prefix");
static EngineStatGpuTimer s_statGlimmerBrickPassGather("Rendering/GPU/Glimmer/BrickPasses/Gather");
static EngineStatGpuTimer s_statGlimmerBrickPassTiles("Rendering/GPU/Glimmer/BrickPasses/Tiles");
static EngineStatGpuTimer s_statGlimmerBrickPassLight("Rendering/GPU/Glimmer/BrickPasses/Light");

// in BuildPass order
static StaticShaderPropertyId s_propBrickModes[] = {
    ShaderProperty(NAME("MODE"), NAME("INIT")),
    ShaderProperty(NAME("MODE"), NAME("BEGIN")),
    ShaderProperty(NAME("MODE"), NAME("CLEAR")),
    ShaderProperty(NAME("MODE"), NAME("BIN")),
    ShaderProperty(NAME("MODE"), NAME("ALLOC")),
    ShaderProperty(NAME("MODE"), NAME("ARGS")),
    ShaderProperty(NAME("MODE"), NAME("COUNT")),
    ShaderProperty(NAME("MODE"), NAME("PREFIX")),
    ShaderProperty(NAME("MODE"), NAME("GATHER")),
    ShaderProperty(NAME("MODE"), NAME("TILES")),
    ShaderProperty(NAME("MODE"), NAME("LIVE")),
    ShaderProperty(NAME("MODE"), NAME("GROW"))
};

static StaticShaderPropertyId s_propBrickLightModeLight { ShaderProperty(NAME("MODE"), NAME("LIGHT")) };
static StaticShaderPropertyId s_propBrickLightModeMips { ShaderProperty(NAME("MODE"), NAME("MIPS")) };
static StaticShaderPropertyId s_propBrickLightModeGather { ShaderProperty(NAME("MODE"), NAME("GATHER")) };

static constexpr uint32 BrickGroupSize = 64;
// NOTE must match GlimmerBricks.hlsli: a word per chunk of every level, then the cells of the levels held
static constexpr uint32 BrickTableChunkWords = GlimmerBrickChunks * GlimmerBrickLevels;
static constexpr uint32 BrickTableLevelSize = GlimmerBrickGridXZ * GlimmerBrickGridY * GlimmerBrickGridXZ * 3;
static constexpr uint32 BrickListNodeCapacity = 1u << 20;
static constexpr uint32 BrickListNodeWords = 3;

static constexpr uint32 PoolBricks = 0;
static constexpr uint32 PoolPages = 1;
static constexpr uint32 PoolCards = 2;

static constexpr uint32 InitialPoolBricks = 4096;
static constexpr uint32 InitialPoolPages = 16384;
static constexpr uint32 InitialPoolCards = 16384;

static constexpr double IdealPagesPerBrick = 3.0;
static constexpr double IdealCardsPerBrick = 2.5;

static constexpr uint32 RelightBricksPerFrame = 128;
static constexpr uint32 RelightTrickleBricks = 16;
static constexpr uint32 RetraceBurstBricks = 32;
static constexpr float RelightBurstChange = 0.05f;

static constexpr float GIRayBiasColumns = 0.5f;

static_assert(InitialPoolBricks <= GlimmerBrickMaxPoolBricks && InitialPoolPages <= GlimmerBrickMaxPoolPages && InitialPoolCards <= GlimmerBrickMaxPoolCards);

// NOTE must match GlimmerBricks.hlsli
static constexpr uint32 BrickNumCounters = 16;
static constexpr uint32 BrickCounterFreeBricks = 0;
static constexpr uint32 BrickCounterFreePages = 6;
static constexpr uint32 BrickCounterFreeCards = 7;
static constexpr uint32 BrickCounterBatchDroppedNodes = 15;
static constexpr uint32 BrickSlotCounters = 8;
static constexpr uint32 BrickSlotFailedBricks = 0;
static constexpr uint32 BrickSlotFailedPages = 1;
static constexpr uint32 BrickSlotFailedCards = 2;
static constexpr uint32 BrickSlotBricks = 3;
static constexpr uint32 BrickSlotPages = 4;
static constexpr uint32 BrickSlotCards = 5;
static constexpr uint32 BrickColumnsPerBrick = GlimmerBrickSide * GlimmerBrickSide;
static constexpr uint32 BrickChunkBricks = GlimmerBrickChunkCells * GlimmerBrickChunkCells * GlimmerBrickChunkCells * 3;
// the counters, then each slot's counters, each slot's list heads, then the batch's build list of two words a brick (NOTE must match GlimmerBricks.hlsli)
static constexpr uint32 BrickCountersWords = BrickNumCounters + GlimmerBrickMaxChunkBuildsPerFrame * (BrickSlotCounters + BrickChunkBricks * 3);
static constexpr uint32 BrickReadbackWords = BrickNumCounters + GlimmerBrickMaxChunkBuildsPerFrame * BrickSlotCounters;

// NOTE must match GlimmerBrickBuild.hlsl
static constexpr uint32 BrickArgsColumns = 0;
static constexpr uint32 BrickArgsTiles = 3;
static constexpr uint32 BrickArgsPrefix = 6;
static constexpr uint32 BrickArgsMips = 9;
static constexpr uint32 BrickArgsWords = BrickArgsMips + 3 * (GlimmerBrickMips - GlimmerBrickCardFirstMip);

static constexpr int32 WindowRecenterChunks = 2;
static constexpr uint32 MaxEmptyChunkClearsPerFrame = 32;
static constexpr uint32 PoolLogIntervalFrames = 300;
static constexpr uint32 MinPoolIncrease = 64;
static constexpr uint32 LightViewFrames = 2;
static constexpr int32 ChunkCells = int32(GlimmerBrickChunkCells);
static constexpr float ChangeMargin = 0.25f;
static constexpr float ShadowBiasColumns = 0.5f;
static constexpr float ShadowDistance = 500.0f;

static const Vec3i ChunkDims = Vec3i(int32(GlimmerBrickChunksXZ), int32(GlimmerBrickChunksY), int32(GlimmerBrickChunksXZ));
static const Vec3i HalfChunkDims = Vec3i(ChunkDims.x / 2, ChunkDims.y / 2, ChunkDims.z / 2);

// a level whose bricks don't all fit is pulled in towards the viewer, the finest first, and let out again once the pools have room for a while
static constexpr int32 MinReachChunks = 2;
static constexpr uint32 ReachChangeFrames = 15;
static constexpr uint32 ReachGrowFrames = 600;
static constexpr double ReachGrowFreeShare = 0.4;

struct GlimmerBrickBuildConstants
{
    GlimmerBrickShaderData bricks;
    Vec4u level;   // x = the level of the batch's chunks, or of the cells cleared; y = levels the table holds
    Vec4i boxMin;
    Vec4i boxExtent;
    Vec4u counts;  // x = span chunks, y = groups along x, z = pool bricks, w = list node capacity
    Vec4u options; // x = 1 to sample the textures, y = the LIVE step, z = chunks in the batch, w = 1 to give bricks cards
    Vec4i chunkMins[GlimmerBrickMaxChunkBuildsPerFrame];
};

struct GlimmerBrickLightConstants
{
    GlimmerBrickShaderData bricks;
    Vec4u range;  // x = first place in the live list, y = bricks, z = 1 when they are the build list's, w = mip level
    Vec4f params; // x = columns off its surface a shadow ray starts, y = how far a shadow ray looks, z = 1 when the indirect light is the slots' (brick GI), w = foliage extinction
    Vec4u info;   // x = 1 when the ground cache can be read, y = 1 when a shadow ray goes on through the heightfield
    GlimmerLightShaderData light;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerRelightShaderData lit; // the ground cache's lit windows
};

struct GlimmerBrickGITraceConstants
{
    GlimmerBrickShaderData bricks;
    Vec4u range;  // x = first place in the live list, y = bricks, z = 1 when they are the build list's, w = 1 when the ground and spans can be traced
    Vec4f params; // x = columns off its surface a ray starts, y = how far a ray looks, z = foliage extinction, w = albedo where the ground's isn't known
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
};

struct GlimmerBrickDebugConstants
{
    Vec4u dimensionsMode;
    Vec4f params;
    Vec4u info;
    GlimmerBrickShaderData bricks;
};

#pragma region GlimmerBrickWorld

Vec3u GlimmerBrickWorld::s_largestPools = Vec3u::Zero();

const GlimmerBrickWorld::PoolBuffer GlimmerBrickWorld::s_poolBuffers[] = {
    { &GlimmerBrickWorld::m_indexBuffer, PoolBricks, GlimmerBrickIndexStride, PoolUse::Always, NAME("GlimmerBrickIndex") },
    { &GlimmerBrickWorld::m_freeListBuffer, PoolBricks, 1, PoolUse::Always, NAME("GlimmerBrickFreeList") },
    { &GlimmerBrickWorld::m_liveListBuffer, PoolBricks, 1, PoolUse::Always, NAME("GlimmerBrickLiveList") },
    { &GlimmerBrickWorld::m_crossingsBuffer, PoolPages, GlimmerBrickPageCrossings * 2, PoolUse::Always, NAME("GlimmerBrickCrossings") },
    { &GlimmerBrickWorld::m_materialBuffer, PoolPages, GlimmerBrickPageCrossings, PoolUse::Always, NAME("GlimmerBrickMaterial") },
    { &GlimmerBrickWorld::m_pageFreeListBuffer, PoolPages, 1, PoolUse::Always, NAME("GlimmerBrickPageFreeList") },
    { &GlimmerBrickWorld::m_crossingLightBuffer, PoolPages, GlimmerBrickPageCrossings, PoolUse::Lit, NAME("GlimmerBrickCrossingLight") },
    { &GlimmerBrickWorld::m_cardFreeListBuffer, PoolCards, 1, PoolUse::Always, NAME("GlimmerBrickCardFreeList") },
    { &GlimmerBrickWorld::m_radianceBuffer, PoolCards, GlimmerBrickCardTexels, PoolUse::Lit, NAME("GlimmerBrickRadiance") },
    { &GlimmerBrickWorld::m_slotsBuffer, PoolCards, GlimmerBrickGITiles * GlimmerBrickGIRays, PoolUse::GI, NAME("GlimmerBrickSlots") },
    { &GlimmerBrickWorld::m_arrivingBuffer, PoolCards, GlimmerBrickCardTexels, PoolUse::GI, NAME("GlimmerBrickArriving") }
};

GlimmerBrickWorld::GlimmerBrickWorld()
    : m_framesSinceLightView(LightViewFrames),
      m_isTextured(g_cvGlimmerBricksTextured.Get())
{
    for (uint32 slot = 0; slot < m_chunks.Size(); slot++)
    {
        m_chunks[slot].level = slot / GlimmerBrickChunks;
    }
}

GlimmerBrickWorld::~GlimmerBrickWorld()
{
    for (const PoolBuffer& poolBuffer : s_poolBuffers)
    {
        EnqueueDeletion(std::move(this->*poolBuffer.buffer));
    }

    EnqueueDeletion(std::move(m_tableBuffer));
    EnqueueDeletion(std::move(m_countersBuffer));
    EnqueueDeletion(std::move(m_listNodesBuffer));
    EnqueueDeletion(std::move(m_dispatchArgsBuffer));
    EnqueueDeletion(std::move(m_placeholderBuffer));

    for (GpuBufferRef& readbackBuffer : m_readbackBuffers)
    {
        EnqueueDeletion(std::move(readbackBuffer));
    }
}

uint32 GlimmerBrickWorld::ChunkSlot(uint32 level, const Vec3i& coord)
{
    const uint32 x = uint32(coord.x) & (GlimmerBrickChunksXZ - 1);
    const uint32 y = uint32(coord.y) & (GlimmerBrickChunksY - 1);
    const uint32 z = uint32(coord.z) & (GlimmerBrickChunksXZ - 1);

    return level * GlimmerBrickChunks + (z * GlimmerBrickChunksY + y) * GlimmerBrickChunksXZ + x;
}

float GetGlimmerBrickCellSize()
{
    return MathUtil::Clamp(g_cvGlimmerBricksCellSize.Get(), 0.5f, 32.0f);
}

uint32 GetGlimmerBrickLevelScale()
{
    return g_cvGlimmerBricksLevelScale.Get() >= 4 ? 4u : 2u;
}

float GlimmerBrickWorld::LevelCellSize(uint32 level) const
{
    float size = m_cellSize;

    for (uint32 finer = 0; finer < level; finer++)
    {
        size *= float(m_levelScale);
    }

    return size;
}

float GlimmerBrickWorld::LevelChunkSize(uint32 level) const
{
    return LevelCellSize(level) * float(GlimmerBrickChunkCells);
}

Vec3f GlimmerBrickWorld::ChunkCenter(uint32 level, const Vec3i& coord) const
{
    return (Vec3f(coord) + Vec3f(0.5f)) * LevelChunkSize(level);
}

static uint32 GetBrickLevelCount()
{
    return uint32(MathUtil::Clamp(g_cvGlimmerBricksLevels.Get(), 1, int32(GlimmerBrickLevels)));
}

static GpuBufferRef CreateBrickBuffer(size_t numWords, Name name)
{
    GpuBufferRef buffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, numWords * sizeof(uint32), alignof(uint32));
    Check(buffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
    buffer->SetDebugName(name);
#endif

    return buffer;
}

// a crossing's light and a card's texels are only held while lit, and the GI's slots and what arrives only with brick GI
bool GlimmerBrickWorld::IsPoolUseHeld(PoolUse use, bool isLit, bool isGI)
{
    return use == PoolUse::Always || (use == PoolUse::Lit && isLit) || (use == PoolUse::GI && isGI);
}

uint64 GlimmerBrickWorld::PoolBytes(const Vec3u& pools, bool isLit, bool isGI)
{
    uint64 numWords = 0;

    for (const PoolBuffer& poolBuffer : s_poolBuffers)
    {
        numWords += IsPoolUseHeld(poolBuffer.use, isLit, isGI) ? uint64(pools[poolBuffer.pool]) * poolBuffer.wordsPerItem : 0;
    }

    return numWords * sizeof(uint32);
}

Vec3u GlimmerBrickWorld::GetInitialPools() const
{
    const Vec3u maxima = Vec3u(GlimmerBrickMaxPoolBricks, GlimmerBrickMaxPoolPages, GlimmerBrickMaxPoolCards);
    const uint64 budgetBytes = uint64(MathUtil::Max(g_cvGlimmerBricksPoolBudgetMB.Get(), 0)) * 1024ull * 1024ull;
    const uint64 initialBytes = MathUtil::Min(uint64(MathUtil::Max(g_cvGlimmerBricksInitialPoolMB.Get(), 0)) * 1024ull * 1024ull, budgetBytes);

    Vec3u pools = Vec3u(InitialPoolBricks, InitialPoolPages, InitialPoolCards);

    if (initialBytes != 0)
    {
        const auto getIdeal = [&](uint32 bricks)
        {
            return Vec3u(
                bricks,
                uint32(MathUtil::Min(std::ceil(IdealPagesPerBrick * double(bricks)), double(maxima.y))),
                uint32(MathUtil::Min(std::ceil(IdealCardsPerBrick * double(bricks)), double(maxima.z))));
        };

        uint32 low = 0;
        uint32 high = maxima.x;

        while (low < high)
        {
            const uint32 middle = low + (high - low + 1) / 2;

            if (PoolBytes(getIdeal(middle), true, false) <= initialBytes)
            {
                low = middle;
            }
            else
            {
                high = middle - 1;
            }
        }

        const Vec3u sized = getIdeal(low);

        for (int pool = 0; pool < 3; pool++)
        {
            pools[pool] = MathUtil::Max(pools[pool], sized[pool]);
        }
    }

    for (int pool = 0; pool < 3; pool++)
    {
        pools[pool] = MathUtil::Min(MathUtil::Max(pools[pool], s_largestPools[pool]), maxima[pool]);
    }

    return pools;
}

void GlimmerBrickWorld::CreateTable()
{
    EnqueueDeletion(std::move(m_tableBuffer));

    m_tableLevels = GetBrickLevelCount();
    m_cellSize = GetGlimmerBrickCellSize();
    m_levelScale = GetGlimmerBrickLevelScale();
    m_isBalanced = false;
    m_tableBuffer = CreateBrickBuffer(BrickTableChunkWords + size_t(BrickTableLevelSize) * m_tableLevels, NAME("GlimmerBrickTable"));
}

// every level's window is given up, and the table and pools are as new
void GlimmerBrickWorld::ResetWindows(Frame* frame)
{
    for (Chunk& chunk : m_chunks)
    {
        chunk.Empty();
        chunk.isBuilt = false;
        chunk.isDirty = false;
        chunk.isStarved = false;
        chunk.hasGeometry = false;
        chunk.buildSerial++;
    }

    for (uint32 level = 0; level < GlimmerBrickLevels; level++)
    {
        m_hasWindows[level] = false;
        m_numDirtyChunks[level] = 0;
        const float cellSize = LevelCellSize(level);
        const float columnWidth = cellSize / float(GlimmerBrickSide);

        m_shaderData.levels[level].origin = Vec4i::Zero();
        m_shaderData.levels[level].params = Vec4f(cellSize, 1.0f / cellSize, columnWidth, 1.0f / columnWidth);
        m_reachChunks[level] = HalfChunkDims.x;
    }

    m_numLevels = m_tableLevels;

    Dispatch(frame, BuildPass::Init, Box {}, MathUtil::Max(MathUtil::Max(BrickTableChunkWords + BrickTableLevelSize * m_tableLevels, m_pools.z), MathUtil::Max(m_pools.x, m_pools.y)));
}

void GlimmerBrickWorld::CreateResources()
{
    CreateTable();

    m_countersBuffer = CreateBrickBuffer(BrickCountersWords, NAME("GlimmerBrickCounters"));
    m_listNodesBuffer = CreateBrickBuffer(size_t(BrickListNodeCapacity) * BrickListNodeWords, NAME("GlimmerBrickListNodes"));
    m_placeholderBuffer = CreateBrickBuffer(4, NAME("GlimmerBrickPlaceholder"));

    m_dispatchArgsBuffer = RI.MakeGpuBuffer(GpuBufferType::IndirectArgsBuffer, BrickArgsWords * sizeof(uint32), alignof(uint32));
    Check(m_dispatchArgsBuffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
    m_dispatchArgsBuffer->SetDebugName(NAME("GlimmerBrickDispatchArgs"));
#endif

    m_pools = GetInitialPools();

    CreatePoolBuffers(PoolUse::Always);

    if (m_isLit)
    {
        CreatePoolBuffers(PoolUse::Lit);
    }

    if (m_isGI)
    {
        CreatePoolBuffers(PoolUse::GI);
    }
}

void GlimmerBrickWorld::CreatePoolBuffers(PoolUse use)
{
    for (const PoolBuffer& poolBuffer : s_poolBuffers)
    {
        if (poolBuffer.use == use)
        {
            this->*poolBuffer.buffer = CreateBrickBuffer(size_t(m_pools[poolBuffer.pool]) * poolBuffer.wordsPerItem, poolBuffer.name);
        }
    }
}

void GlimmerBrickWorld::ReleasePoolBuffers(PoolUse use)
{
    for (const PoolBuffer& poolBuffer : s_poolBuffers)
    {
        if (poolBuffer.use == use)
        {
            EnqueueDeletion(std::move(this->*poolBuffer.buffer));
        }
    }
}

void GlimmerBrickWorld::GrowPoolResources(Frame* frame, const Vec3u& grown)
{
    CommandRecorder& cr = frame->cr;

    const Vec3u old = m_pools;

    for (const PoolBuffer& poolBuffer : s_poolBuffers)
    {
        if (grown[poolBuffer.pool] == old[poolBuffer.pool] || !IsPoolUseHeld(poolBuffer.use, m_isLit, m_isGI))
        {
            continue;
        }

        GpuBufferRef& buffer = this->*poolBuffer.buffer;
        GpuBufferRef grownBuffer = CreateBrickBuffer(size_t(grown[poolBuffer.pool]) * poolBuffer.wordsPerItem, poolBuffer.name);

        cr << InsertBarrier(buffer.Get(), ResourceState::CopySrc, ShaderModuleType::Compute);
        cr << InsertBarrier(grownBuffer.Get(), ResourceState::CopyDst, ShaderModuleType::Compute);
        cr << CopyBuffer(buffer.Get(), grownBuffer.Get(), uint32(size_t(old[poolBuffer.pool]) * poolBuffer.wordsPerItem * sizeof(uint32)));

        EnqueueDeletion(std::move(buffer));

        buffer = std::move(grownBuffer);
    }

    m_pools = grown;

    for (int pool = 0; pool < 3; pool++)
    {
        s_largestPools[pool] = MathUtil::Max(s_largestPools[pool], grown[pool]);
    }

    const uint32 numAdded = MathUtil::Max(grown.x - old.x, MathUtil::Max(grown.y - old.y, grown.z - old.z));

    Dispatch(frame, BuildPass::Grow, Box { Vec3i(int32(old.x), int32(old.y), int32(old.z)), Vec3i::Zero() }, numAdded);
}

FixedArray<uint32, GlimmerBrickLevels> GlimmerBrickWorld::CountLevelBricks() const
{
    FixedArray<uint32, GlimmerBrickLevels> counts {};

    for (const Chunk& chunk : m_chunks)
    {
        counts[chunk.level] += chunk.hasBricks ? chunk.numBricks : 0;
    }

    return counts;
}

// the most bricks the budget holds with pages and cards in the given proportion to them, and what they all take while lit and with GI as they are now
Vec3u GlimmerBrickWorld::FitPools(double pagesPerBrick, double cardsPerBrick, uint64 budgetBytes) const
{
    const Vec3u maxima = Vec3u(GlimmerBrickMaxPoolBricks, GlimmerBrickMaxPoolPages, GlimmerBrickMaxPoolCards);

    const auto balancedAt = [&](uint32 bricks)
    {
        return Vec3u(
            bricks,
            uint32(MathUtil::Min(std::ceil(pagesPerBrick * double(bricks)), double(maxima.y))),
            uint32(MathUtil::Min(std::ceil(cardsPerBrick * double(bricks)), double(maxima.z))));
    };

    uint32 low = 0;
    uint32 high = maxima.x;

    while (low < high)
    {
        const uint32 middle = low + (high - low + 1) / 2;

        if (PoolBytes(balancedAt(middle), m_isLit, m_isGI) <= budgetBytes)
        {
            low = middle;
        }
        else
        {
            high = middle - 1;
        }
    }

    return balancedAt(low);
}

// once the levels settle with one pool used up and another with room to spare, the pools are made again in the proportion the chunks use them, within the budget, and every brick is built again
bool GlimmerBrickWorld::WantsRebalancedPools() const
{
    if (m_isBalanced || m_grownPools != Vec3u::Zero())
    {
        return false;
    }

    bool isFull = false;
    bool hasSpare = false;

    for (int pool = 0; pool < 3; pool++)
    {
        const double used = 1.0 - double(MathUtil::Min(m_freeInPools[pool], m_pools[pool])) / double(MathUtil::Max(m_pools[pool], 1u));

        isFull = isFull || used >= 0.95;
        hasSpare = hasSpare || used <= 0.75;
    }

    return isFull && hasSpare;
}

void GlimmerBrickWorld::RebalancePools()
{
    m_isBalanced = true;

    uint64 heldBricks = 0;
    uint64 heldPages = 0;
    uint64 heldCards = 0;

    for (const Chunk& chunk : m_chunks)
    {
        if (chunk.hasBricks && !chunk.isShort)
        {
            heldBricks += chunk.numBricks;
            heldPages += chunk.numPages;
            heldCards += chunk.numCards;
        }
    }

    if (heldBricks == 0)
    {
        return;
    }

    const uint64 budgetBytes = uint64(MathUtil::Max(g_cvGlimmerBricksPoolBudgetMB.Get(), 0)) * 1024ull * 1024ull;
    const Vec3u fitted = FitPools(double(heldPages) / double(heldBricks), double(heldCards) / double(heldBricks), budgetBytes);

    if (fitted == m_pools)
    {
        return;
    }

    HYP_LOG(Rendering, Info, "Glimmer: brick pools rebalanced from {} bricks, {} pages and {} cards to {}, {} and {} ({} MB)",
        m_pools.x, m_pools.y, m_pools.z, fitted.x, fitted.y, fitted.z, PoolBytes(fitted, m_isLit, m_isGI) / (1024ull * 1024ull));

    for (PoolUse use : { PoolUse::Always, PoolUse::Lit, PoolUse::GI })
    {
        if (IsPoolUseHeld(use, m_isLit, m_isGI))
        {
            ReleasePoolBuffers(use);
        }
    }

    m_pools = fitted;
    s_largestPools = fitted;

    for (PoolUse use : { PoolUse::Always, PoolUse::Lit, PoolUse::GI })
    {
        if (IsPoolUseHeld(use, m_isLit, m_isGI))
        {
            CreatePoolBuffers(use);
        }
    }
}

bool GlimmerBrickWorld::GrowPools(const Vec3u& needed)
{
    const Vec3u current = m_pools;
    const Vec3u maxima = Vec3u(GlimmerBrickMaxPoolBricks, GlimmerBrickMaxPoolPages, GlimmerBrickMaxPoolCards);

    const uint64 budgetBytes = uint64(MathUtil::Max(g_cvGlimmerBricksPoolBudgetMB.Get(), 0)) * 1024ull * 1024ull;

    // no pool grows past its share of the most bricks the budget holds, with pages and cards in the proportion the chunks built in full use them
    Vec3u caps = maxima;

    uint64 heldBricks = 0;
    uint64 heldPages = 0;
    uint64 heldCards = 0;

    for (const Chunk& chunk : m_chunks)
    {
        if (chunk.hasBricks && !chunk.isShort)
        {
            heldBricks += chunk.numBricks;
            heldPages += chunk.numPages;
            heldCards += chunk.numCards;
        }
    }

    if (heldBricks != 0)
    {
        const double pagesPerBrick = double(heldPages) / double(heldBricks);
        const double cardsPerBrick = double(heldCards) / double(heldBricks);

        const Vec3u balanced = FitPools(pagesPerBrick, cardsPerBrick, budgetBytes);

        for (int pool = 0; pool < 3; pool++)
        {
            caps[pool] = MathUtil::Max(balanced[pool], current[pool]);
        }
    }

    Vec3u grown = m_grownPools == Vec3u::Zero() ? current : m_grownPools;

    for (int pool = 0; pool < 3; pool++)
    {
        if (needed[pool] == 0 || grown[pool] != current[pool])
        {
            continue;
        }

        // twice the size, or what the batch fell short by and a quarter more if that's bigger, or as much more as the budget leaves room for
        const uint64 wanted = MathUtil::Max(uint64(current[pool]) * 2, uint64(current[pool]) + uint64(needed[pool]) + uint64(needed[pool]) / 4);
        uint32 increase = uint32(MathUtil::Min(wanted, uint64(caps[pool]))) - current[pool];

        for (; increase >= MinPoolIncrease; increase /= 2)
        {
            Vec3u candidate = grown;
            candidate[pool] = current[pool] + increase;

            if (PoolBytes(candidate, m_isLit, m_isGI) <= budgetBytes)
            {
                grown = candidate;

                break;
            }
        }
    }

    if (grown == current)
    {
        return m_grownPools != Vec3u::Zero();
    }

    if (grown != m_grownPools)
    {
        const FixedArray<uint32, GlimmerBrickLevels> levelBricks = CountLevelBricks();

        HYP_LOG(Rendering, Info, "Glimmer: brick pools ran short; growing to {} bricks, {} pages and {} cards ({} MB); bricks by level {} {} {} {}",
            grown.x, grown.y, grown.z, PoolBytes(grown, m_isLit, m_isGI) / (1024ull * 1024ull), levelBricks[0], levelBricks[1], levelBricks[2], levelBricks[3]);
    }

    m_grownPools = grown;

    return true;
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
    cr << InsertBarrier(m_liveListBuffer.Get(), state, ShaderModuleType::Compute);

    if (isForWriting)
    {
        cr << InsertBarrier(m_freeListBuffer.Get(), state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_pageFreeListBuffer.Get(), state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_cardFreeListBuffer.Get(), state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_listNodesBuffer.Get(), state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_placeholderBuffer.Get(), state, ShaderModuleType::Compute);
    }
}

void GlimmerBrickWorld::Dispatch(Frame* frame, BuildPass pass, const Box& box, uint32 numThreads, uint32 step)
{
    const Vec3u groups = RenderHelpers::WrapComputeGroupCount((numThreads + BrickGroupSize - 1) / BrickGroupSize);

    SetBuildPass(frame, pass, box, groups.x, step);

    frame->cr << DispatchCompute(groups);
}

void GlimmerBrickWorld::DispatchIndirect(Frame* frame, BuildPass pass, uint32 argsOffset)
{
    SetBuildPass(frame, pass, Box {}, 0, 0);

    frame->cr << DispatchComputeIndirect(m_dispatchArgsBuffer.Get(), argsOffset * sizeof(uint32));
}

uint32 GlimmerBrickWorld::BindBuffers(CommandRecorder& cr, uint32 uniformIndex, EnumFlags<GlimmerBrickBindings> bindings) const
{
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickTableBuffer"_sh, m_tableBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickIndexBuffer"_sh, m_indexBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingsBuffer"_sh, m_crossingsBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickMaterialBuffer"_sh, m_materialBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));

    if (bindings & GlimmerBrickBindings::Counters)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCountersBuffer"_sh, m_countersBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    }

    if (bindings & GlimmerBrickBindings::Light)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickRadianceBuffer"_sh, (m_isLit ? m_radianceBuffer : m_placeholderBuffer).Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingLightBuffer"_sh, (m_isLit ? m_crossingLightBuffer : m_placeholderBuffer).Get(), ShaderDataOffset(0, sizeof(uint32)));
    }

    // without brick GI, any buffer read only already will do for the slots, as nothing reads them
    if (bindings & GlimmerBrickBindings::GI)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickSlotsBuffer"_sh, (m_isGI ? m_slotsBuffer : m_indexBuffer).Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickArrivingBuffer"_sh, (m_isGI ? m_arrivingBuffer : m_placeholderBuffer).Get(), ShaderDataOffset(0, sizeof(uint32)));
    }

    return uniformIndex;
}

void GlimmerBrickWorld::SetBuildPass(Frame* frame, BuildPass pass, const Box& box, uint32 groupsX, uint32 step)
{
    static_assert(std::size(s_propBrickModes) == uint32(BuildPass::Max));

    CommandRecorder& cr = frame->cr;

    GlimmerBrickBuildConstants constants {};
    constants.bricks = m_shaderData;
    constants.level = Vec4u(pass == BuildPass::Clear ? box.level : m_batchLevel, m_tableLevels, 0u, 0u);
    constants.boxMin = Vec4i(box.min, int32(m_pools.z));
    constants.boxExtent = Vec4i(box.extent, int32(m_pools.y));
    constants.counts = Vec4u(m_sceneInstances->GetNumSpanChunks(), groupsX, m_pools.x, BrickListNodeCapacity);
    constants.options = Vec4u(m_isTextured ? 1u : 0u, step, m_numBatchChunks, m_isLit ? 1u : 0u);

    for (uint32 slot = 0; slot < m_numBatchChunks; slot++)
    {
        constants.chunkMins[slot] = m_batchChunkMins[slot];
    }

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(s_propBrickModes[uint32(pass)]);

    InsertBarriers(frame, true);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickBuild"), shaderProperties));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    uniformIndex = BindBuffers(cr, uniformIndex, GlimmerBrickBindings::Counters);

    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickFreeListBuffer"_sh, m_freeListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickPageFreeListBuffer"_sh, m_pageFreeListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCardFreeListBuffer"_sh, m_cardFreeListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickListNodesBuffer"_sh, m_listNodesBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickLiveListBuffer"_sh, m_liveListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "SpanInstancesBuffer"_sh, m_sceneInstances->GetSpanInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanInstanceShaderData)));
    cr << SetShaderUniform(uniformIndex++, "SpanChunksBuffer"_sh, m_sceneInstances->GetSpanChunksBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanChunkShaderData)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerTrianglesBuffer"_sh, m_triangleCache->GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerTriangleTexcoordsBuffer"_sh, m_triangleCache->GetTexcoordsBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());

    // the args are read by the passes they size, so only the args pass writes them
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickDispatchArgsBuffer"_sh, (pass == BuildPass::Args ? m_dispatchArgsBuffer : m_placeholderBuffer).Get(), ShaderDataOffset(0, sizeof(uint32)));
}

void GlimmerBrickWorld::MoveWindow(Frame* frame, uint32 level, const Vec3i& originChunk)
{
    const Vec3i oldOrigin = m_originChunks[level];
    const bool hadWindow = m_hasWindows[level];

    const bool overlapsOld = hadWindow
        && MathUtil::Abs(originChunk.x - oldOrigin.x) < ChunkDims.x
        && MathUtil::Abs(originChunk.y - oldOrigin.y) < ChunkDims.y
        && MathUtil::Abs(originChunk.z - oldOrigin.z) < ChunkDims.z;

    m_originChunks[level] = originChunk;
    if (!overlapsOld)
    {
        // a level that jumps clears all of its old window; the table of one that had none is clear already
        if (hadWindow)
        {
            const Box box { oldOrigin * ChunkCells, ChunkDims * ChunkCells, level };

            Dispatch(frame, BuildPass::Clear, box, uint32(box.extent.x * box.extent.y * box.extent.z) * 3);
        }
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

            const Box box { enteringMin * ChunkCells, (enteringMax - enteringMin) * ChunkCells, level };

            Dispatch(frame, BuildPass::Clear, box, uint32(box.extent.x * box.extent.y * box.extent.z) * 3);
        }
    }

    for (int32 z = 0; z < ChunkDims.z; z++)
    {
        for (int32 y = 0; y < ChunkDims.y; y++)
        {
            for (int32 x = 0; x < ChunkDims.x; x++)
            {
                const Vec3i coord = originChunk + Vec3i(x, y, z);

                Chunk& chunk = m_chunks[ChunkSlot(level, coord)];

                // what was starved may fit now that a slab has left
                chunk.isStarved = false;

                if (overlapsOld && chunk.coord == coord)
                {
                    continue;
                }

                chunk.Empty();
                chunk.coord = coord;
                chunk.isBuilt = false;
                chunk.isDirty = true;
                chunk.buildSerial++;
            }
        }
    }

    m_hasWindows[level] = true;

    SetLevelOrigin(level);
    RefreshReach(level);
}

void GlimmerBrickWorld::GetReachBox(uint32 level, Vec3i& outMin, Vec3i& outMax) const
{
    const Vec3i center = m_originChunks[level] + HalfChunkDims;
    const int32 reachXZ = m_reachChunks[level];
    const Vec3i reach = Vec3i(reachXZ, MathUtil::Min(reachXZ, HalfChunkDims.y), reachXZ);

    outMin = center - reach;
    outMax = center + reach;
}

bool GlimmerBrickWorld::IsInReach(const Chunk& chunk) const
{
    if (!m_hasWindows[chunk.level])
    {
        return false;
    }

    Vec3i reachMin;
    Vec3i reachMax;
    GetReachBox(chunk.level, reachMin, reachMax);

    return chunk.coord.x >= reachMin.x && chunk.coord.y >= reachMin.y && chunk.coord.z >= reachMin.z
        && chunk.coord.x < reachMax.x && chunk.coord.y < reachMax.y && chunk.coord.z < reachMax.z;
}

// a coarse chunk wholly inside the finer level's reach: traces and receivers use the finer level there
bool GlimmerBrickWorld::IsCoveredByFiner(const Chunk& chunk) const
{
    if (chunk.level == 0 || !m_hasWindows[chunk.level - 1])
    {
        return false;
    }

    Vec3i reachMin;
    Vec3i reachMax;
    GetReachBox(chunk.level - 1, reachMin, reachMax);

    const float finerChunkSize = LevelChunkSize(chunk.level - 1);
    const Vec3f finerMin = Vec3f(reachMin) * finerChunkSize;
    const Vec3f finerMax = Vec3f(reachMax) * finerChunkSize;

    const float chunkSize = LevelChunkSize(chunk.level);
    const Vec3f chunkMin = Vec3f(chunk.coord) * chunkSize;
    const Vec3f chunkMax = chunkMin + Vec3f(chunkSize);

    return chunkMin.x >= finerMin.x && chunkMin.y >= finerMin.y && chunkMin.z >= finerMin.z
        && chunkMax.x <= finerMax.x && chunkMax.y <= finerMax.y && chunkMax.z <= finerMax.z;
}

// the shaders see a level's reach as its window
void GlimmerBrickWorld::SetLevelOrigin(uint32 level)
{
    Vec3i reachMin;
    Vec3i reachMax;
    GetReachBox(level, reachMin, reachMax);

    const Vec3i extent = reachMax - reachMin;

    m_shaderData.levels[level].origin = Vec4i(reachMin * ChunkCells, extent.x | (extent.y << 8));
}

// what has left the reach is cleared, and what the reach has taken in again is built
void GlimmerBrickWorld::RefreshReach(uint32 level)
{
    for (uint32 slot = level * GlimmerBrickChunks; slot < (level + 1) * GlimmerBrickChunks; slot++)
    {
        Chunk& chunk = m_chunks[slot];

        if (!IsInReach(chunk))
        {
            chunk.isDirty = chunk.isDirty || chunk.hasBricks;
        }
        else if (chunk.hasGeometry && !chunk.hasBricks && !chunk.isDirty)
        {
            chunk.isDirty = true;
            chunk.isStarved = false;
        }
    }
}

void GlimmerBrickWorld::UpdateReach()
{
    m_framesSinceReachChange++;

    FixedArray<uint32, GlimmerBrickLevels> levelStarved {};
    uint32 numStarved = 0;
    uint32 numDirty = 0;

    for (const Chunk& chunk : m_chunks)
    {
        if (!chunk.hasGeometry || !IsInReach(chunk))
        {
            continue;
        }

        const bool isStarved = chunk.isStarved && !IsCoveredByFiner(chunk);

        levelStarved[chunk.level] += isStarved ? 1 : 0;
        numStarved += isStarved ? 1 : 0;
        numDirty += chunk.isDirty ? 1 : 0;
    }

    m_framesWithoutStarving = numStarved == 0 ? m_framesWithoutStarving + 1 : 0;

    if (m_framesSinceReachChange < ReachChangeFrames)
    {
        return;
    }

    const auto changeReach = [&](uint32 level, int32 delta)
    {
        m_reachChunks[level] += delta;
        m_framesSinceReachChange = 0;

        SetLevelOrigin(level);
        RefreshReach(level);

        // what starved may fit now
        for (Chunk& chunk : m_chunks)
        {
            chunk.isStarved = false;
        }

        HYP_LOG(Rendering, Info, "Glimmer: brick level {} reach now {} m across", level, float(2 * m_reachChunks[level]) * LevelChunkSize(level));
    };

    // the pools can't grow, and chunks the bricks are traced in went without: the level with the most of them gives up its edge.
    // room is given up by distance in each level's own chunks, so that is the level farthest out in its own terms
    if (numStarved != 0 && m_grownPools == Vec3u::Zero())
    {
        uint32 shrinking = GlimmerBrickLevels;

        for (uint32 level = 0; level < m_numLevels; level++)
        {
            if (m_reachChunks[level] > MinReachChunks && levelStarved[level] != 0 && (shrinking == GlimmerBrickLevels || levelStarved[level] > levelStarved[shrinking]))
            {
                shrinking = level;
            }
        }

        if (shrinking != GlimmerBrickLevels)
        {
            changeReach(shrinking, -1);
        }

        return;
    }

    const bool hasRoom = double(m_freeInPools.x) >= ReachGrowFreeShare * double(m_pools.x)
        && double(m_freeInPools.y) >= ReachGrowFreeShare * double(m_pools.y)
        && double(m_freeInPools.z) >= ReachGrowFreeShare * double(m_pools.z);

    if (hasRoom && numDirty == 0 && m_framesWithoutStarving >= ReachGrowFrames && m_framesSinceReachChange >= ReachGrowFrames)
    {
        for (uint32 level = 0; level < m_numLevels; level++)
        {
            if (m_reachChunks[level] < HalfChunkDims.x)
            {
                changeReach(level, 1);

                return;
            }
        }
    }
}

void GlimmerBrickWorld::MarkAllDirty()
{
    for (Chunk& chunk : m_chunks)
    {
        if (!m_hasWindows[chunk.level])
        {
            continue;
        }

        chunk.isDirty = true;
        chunk.isStarved = false;
    }
}

void GlimmerBrickWorld::MarkSceneChanges()
{
    const uint32 generation = m_sceneInstances->GetGeneration();

    if (generation == m_seenGeneration)
    {
        return;
    }

    GlimmerSceneChanges changes;

    const bool isEverywhere = m_seenGeneration == ~0u || !m_sceneInstances->GetChangesSince(m_seenGeneration, changes);
    m_seenGeneration = generation;

    if (isEverywhere)
    {
        MarkAllDirty();

        return;
    }

    for (Chunk& chunk : m_chunks)
    {
        if (!m_hasWindows[chunk.level])
        {
            continue;
        }

        const float chunkSize = LevelChunkSize(chunk.level);
        const float margin = ChangeMargin * float(1u << chunk.level);

        const Vec3f chunkMin = Vec3f(chunk.coord) * chunkSize - Vec3f(margin);
        const Vec3f chunkMax = chunkMin + Vec3f(chunkSize + 2.0f * margin);

        for (const GlimmerSceneChange& change : changes)
        {
            if ((chunk.isDirty && !chunk.isStarved) || change.isLodOnly)
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

void GlimmerBrickWorld::MarkOccupiedChunks()
{
    for (Chunk& chunk : m_chunks)
    {
        chunk.hasGeometry = false;
    }

    for (uint32 level = 0; level < m_numLevels; level++)
    {
        if (!m_hasWindows[level])
        {
            continue;
        }

        const float chunkSize = LevelChunkSize(level);
        const float margin = ChangeMargin * float(1u << level);

        const Vec3i originChunk = m_originChunks[level];
        const Vec3i windowMax = originChunk + ChunkDims - Vec3i(1);

        for (const GlimmerSceneInstances::SpanKey& spanKey : m_sceneInstances->GetSpanKeys())
        {
            const Vec3f boundsMin = (spanKey.bounds.min - Vec3f(margin)) / chunkSize;
            const Vec3f boundsMax = (spanKey.bounds.max + Vec3f(margin)) / chunkSize;

            Vec3i chunkMin;
            Vec3i chunkMax;

            for (int axis = 0; axis < 3; axis++)
            {
                chunkMin[axis] = MathUtil::Max(int32(MathUtil::Floor(boundsMin[axis])), originChunk[axis]);
                chunkMax[axis] = MathUtil::Min(int32(MathUtil::Floor(boundsMax[axis])), windowMax[axis]);
            }

            for (int32 z = chunkMin.z; z <= chunkMax.z; z++)
            {
                for (int32 y = chunkMin.y; y <= chunkMax.y; y++)
                {
                    for (int32 x = chunkMin.x; x <= chunkMax.x; x++)
                    {
                        m_chunks[ChunkSlot(level, Vec3i(x, y, z))].hasGeometry = true;
                    }
                }
            }
        }
    }
}

void GlimmerBrickWorld::BuildChunks(Frame* frame, Chunk* const* chunks, uint32 numChunks, const GlimmerTechnique* technique)
{
    m_numBatchChunks = numChunks;
    m_batchLevel = chunks[0]->level;

    for (uint32 slot = 0; slot < numChunks; slot++)
    {
        m_batchChunkMins[slot] = Vec4i(chunks[slot]->coord * ChunkCells, 0);
    }

    {
        ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassClear);

        Dispatch(frame, BuildPass::Begin, Box {}, numChunks * BrickChunkBricks);

        for (uint32 slot = 0; slot < numChunks; slot++)
        {
            Dispatch(frame, BuildPass::Clear, Box { m_batchChunkMins[slot].GetXYZ(), Vec3i(ChunkCells), m_batchLevel }, BrickChunkBricks);
        }
    }

    const uint32 numSpanChunks = m_sceneInstances->GetNumSpanChunks();

    if (numSpanChunks != 0)
    {
        CommandRecorder& cr = frame->cr;

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassBin);

            // a group per span chunk
            Dispatch(frame, BuildPass::Bin, Box {}, numSpanChunks * BrickGroupSize);
        }

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassAlloc);

            Dispatch(frame, BuildPass::Alloc, Box {}, numChunks * BrickChunkBricks);

            cr << InsertBarrier(m_dispatchArgsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

            Dispatch(frame, BuildPass::Args, Box {}, 1);

            cr << InsertBarrier(m_dispatchArgsBuffer.Get(), ResourceState::IndirectArg);
        }

        // only as many groups as the bricks taken need
        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassCount);

            DispatchIndirect(frame, BuildPass::Count, BrickArgsColumns);
        }

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassPrefix);

            DispatchIndirect(frame, BuildPass::Prefix, BrickArgsPrefix);
        }

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassGather);

            DispatchIndirect(frame, BuildPass::Gather, BrickArgsColumns);
        }

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassTiles);

            DispatchIndirect(frame, BuildPass::Tiles, BrickArgsTiles);
        }

        if (m_isGI)
        {
            TraceSlots(frame, 0, 0, true);
        }

        if (m_isLit)
        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassLight);

            LightBricks(frame, technique, 0, 0, true);
        }
    }

    for (uint32 slot = 0; slot < numChunks; slot++)
    {
        Chunk& chunk = *chunks[slot];

        chunk.isBuilt = true;
        chunk.isDirty = false;
        chunk.hasBricks = true;
        chunk.buildSerial++;
    }

    CopyBuildCounters(frame, chunks, numChunks);

    m_numBatchChunks = 0;
}

void GlimmerBrickWorld::ClearChunk(Frame* frame, Chunk& chunk)
{
    Dispatch(frame, BuildPass::Clear, Box { chunk.coord * ChunkCells, Vec3i(ChunkCells), chunk.level }, BrickChunkBricks);

    chunk.Empty();
}

void GlimmerBrickWorld::CopyBuildCounters(Frame* frame, Chunk* const* chunks, uint32 numChunks)
{
    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;

    if (!m_readbackBuffers[slot].IsValid())
    {
        m_readbackBuffers[slot] = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, BrickReadbackWords * sizeof(uint32));
        m_readbackBuffers[slot]->SetIsCpuAccessible(true);
        Check(m_readbackBuffers[slot]->Create());
    }

    CommandRecorder& cr = frame->cr;

    // the counters, then each slot's
    const uint32 numBytes = (BrickNumCounters + numChunks * BrickSlotCounters) * sizeof(uint32);

    cr << InsertBarrier(m_countersBuffer.Get(), ResourceState::CopySrc, ShaderModuleType::Compute);
    cr << InsertBarrier(m_readbackBuffers[slot].Get(), ResourceState::CopyDst, ShaderModuleType::Compute);
    cr << CopyBuffer(m_countersBuffer.Get(), m_readbackBuffers[slot].Get(), 0, 0, numBytes);
    cr << InsertBarrier(m_countersBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    for (uint32 buildIndex = 0; buildIndex < numChunks; buildIndex++)
    {
        m_pendingBuilds[slot][buildIndex] = ChunkBuild { chunks[buildIndex]->coord, chunks[buildIndex]->level, chunks[buildIndex]->buildSerial };
    }

    m_numPendingBuilds[slot] = numChunks;
}

bool GlimmerBrickWorld::ReadBuildCounters(Frame* frame, const Vec3f& viewerPosition)
{
    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;
    const uint32 numPending = m_numPendingBuilds[slot];

    m_numPendingBuilds[slot] = 0;

    // this frame slot's last submission has finished by now, so what it copied can be read
    if (numPending == 0)
    {
        return false;
    }

    FixedArray<uint32, BrickReadbackWords> counters;

    m_readbackBuffers[slot]->Invalidate();
    m_readbackBuffers[slot]->Read((BrickNumCounters + numPending * BrickSlotCounters) * sizeof(uint32), counters.Data());

    // the batch ran out of list nodes: its chunks are built again, one at a time
    m_freeInPools = Vec3u(counters[BrickCounterFreeBricks], counters[BrickCounterFreePages], counters[BrickCounterFreeCards]);

    const bool isOverflowed = counters[BrickCounterBatchDroppedNodes] != 0 && numPending > 1;

    if (isOverflowed)
    {
        m_numSoloBuilds = MathUtil::Max(m_numSoloBuilds, numPending);
    }

    const auto isCurrent = [&](uint32 buildIndex, const Chunk& chunk)
    {
        const ChunkBuild& build = m_pendingBuilds[slot][buildIndex];

        return chunk.coord == build.coord
            && chunk.level == build.level
            && chunk.buildSerial == build.buildSerial
            && !chunk.isDirty;
    };

    Vec3u batchNeeded = Vec3u::Zero();

    for (uint32 buildIndex = 0; buildIndex < numPending && !isOverflowed; buildIndex++)
    {
        const Chunk& chunk = m_chunks[ChunkSlot(m_pendingBuilds[slot][buildIndex].level, m_pendingBuilds[slot][buildIndex].coord)];

        if (isCurrent(buildIndex, chunk))
        {
            const uint32* record = counters.Data() + BrickNumCounters + size_t(buildIndex) * BrickSlotCounters;

            batchNeeded += Vec3u(record[BrickSlotFailedBricks], record[BrickSlotFailedPages], record[BrickSlotFailedCards]);
        }
    }

    const bool isGrowing = batchNeeded != Vec3u::Zero() && GrowPools(batchNeeded);

    bool hasWritten = false;

    for (uint32 buildIndex = 0; buildIndex < numPending; buildIndex++)
    {
        Chunk& chunk = m_chunks[ChunkSlot(m_pendingBuilds[slot][buildIndex].level, m_pendingBuilds[slot][buildIndex].coord)];

        if (!isCurrent(buildIndex, chunk))
        {
            continue;
        }

        const uint32* record = counters.Data() + BrickNumCounters + size_t(buildIndex) * BrickSlotCounters;

        chunk.numBricks = record[BrickSlotBricks];
        chunk.numPages = record[BrickSlotPages];
        chunk.numCards = record[BrickSlotCards];

        if (isOverflowed)
        {
            chunk.isDirty = true;

            continue;
        }

        const Vec3u needed = Vec3u(record[BrickSlotFailedBricks], record[BrickSlotFailedPages], record[BrickSlotFailedCards]);

        chunk.isShort = needed != Vec3u::Zero();

        if (needed.x == 0 && needed.y == 0 && needed.z == 0)
        {
            continue;
        }

        // it keeps what it got until it's built again
        chunk.isDirty = true;

        if (isGrowing)
        {
            continue;
        }

        if (MakeRoom(frame, chunk, viewerPosition, needed))
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


// how soon a chunk is built, or how late it gives up its room: by its distance in its level's chunks, and a coarse chunk the finer level's window holds after the rest
float GlimmerBrickWorld::BuildOrder(const Chunk& chunk, const Vec3f& focus) const
{
    const float chunkSize = LevelChunkSize(chunk.level);

    float order = ChunkCenter(chunk.level, chunk.coord).Distance(focus) / chunkSize;

    if (IsCoveredByFiner(chunk))
    {
        order += 0.5f * float(ChunkDims.x);
    }

    return order;
}

bool GlimmerBrickWorld::MakeRoom(Frame* frame, const Chunk& requester, const Vec3f& viewerPosition, const Vec3u& needed)
{
    struct Candidate
    {
        Chunk* chunk;
        float distance;
    };

    // only from chunks a chunk farther, so two at about the same distance don't take turns
    const float minDistance = BuildOrder(requester, viewerPosition) + 1.0f;

    Array<Candidate, GlimmerTempAllocator> candidates;
    candidates.Reserve(m_chunks.Size());

    for (Chunk& chunk : m_chunks)
    {
        if (!chunk.hasBricks || (chunk.numBricks == 0 && chunk.numPages == 0 && chunk.numCards == 0))
        {
            continue;
        }

        const float distance = BuildOrder(chunk, viewerPosition);

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

        ClearChunk(frame, victim);

        victim.isDirty = true;
        victim.isStarved = true;
    }

    m_numEvictions += uint32(numVictims);

    return true;
}

void GlimmerBrickWorld::Update(
    Frame* frame,
    const Vec3f& viewerPosition,
    const Vec3f& lookAhead,
    float budgetScale,
    const GlimmerSceneInstances& sceneInstances,
    const GlimmerTriangleCache& triangleCache,
    const GlimmerSurfaceCache* surfaceCache,
    const GlimmerSpanCache* spanCache,
    const GlimmerGroundGI* groundGI,
    const GlimmerTechnique* technique,
    const GlimmerLightShaderData& light)
{
    HYP_SCOPE;

    if (!sceneInstances.IsReady() || !triangleCache.IsReady() || !sceneInstances.GetSpanInstancesBuffer().IsValid() || !sceneInstances.GetSpanChunksBuffer().IsValid())
    {
        return;
    }

    m_sceneInstances = &sceneInstances;
    m_triangleCache = &triangleCache;
    m_surfaceCache = surfaceCache;
    m_spanCache = spanCache;
    m_groundGI = groundGI;
    m_light = light;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickBuild);

    bool hasWritten = false;

    if (!m_tableBuffer.IsValid())
    {
        CreateResources();
        ResetWindows(frame);

        hasWritten = true;
    }
    else if (m_tableLevels != GetBrickLevelCount() || m_cellSize != GetGlimmerBrickCellSize() || m_levelScale != GetGlimmerBrickLevelScale())
    {
        CreateTable();
        ResetWindows(frame);

        m_occupancyGeneration = ~0u;
        hasWritten = true;
    }
    else if (m_wantsRebalance)
    {
        m_wantsRebalance = false;

        RebalancePools();
        ResetWindows(frame);

        m_occupancyGeneration = ~0u;
        m_hasRelitLight = false;
        hasWritten = true;
    }

    if (m_grownPools != Vec3u::Zero())
    {
        GrowPoolResources(frame, m_grownPools);

        m_grownPools = Vec3u::Zero();

        hasWritten = true;
    }

    // one level moves a frame, so that their clears and rebuilds are spread out; a level without a window takes one at once
    bool hasMoved = false;

    for (uint32 level = 0; level < m_numLevels; level++)
    {
        const float chunkSize = LevelChunkSize(level);

        const Vec3i viewerChunk = Vec3i(
            int32(MathUtil::Floor(viewerPosition.x / chunkSize)),
            int32(MathUtil::Floor(viewerPosition.y / chunkSize)),
            int32(MathUtil::Floor(viewerPosition.z / chunkSize)));

        const Vec3i originChunk = viewerChunk - Vec3i(ChunkDims.x / 2, ChunkDims.y / 2, ChunkDims.z / 2);
        const Vec3i oldOrigin = m_originChunks[level];

        const bool isFar = MathUtil::Abs(originChunk.x - oldOrigin.x) >= WindowRecenterChunks
            || MathUtil::Abs(originChunk.y - oldOrigin.y) >= WindowRecenterChunks
            || MathUtil::Abs(originChunk.z - oldOrigin.z) >= WindowRecenterChunks;

        if (m_hasWindows[level] && (!isFar || hasMoved))
        {
            continue;
        }

        hasMoved = hasMoved || m_hasWindows[level];

        MoveWindow(frame, level, originChunk);

        m_occupancyGeneration = ~0u;

        hasWritten = true;
    }

    if (m_isTextured != g_cvGlimmerBricksTextured.Get())
    {
        m_isTextured = g_cvGlimmerBricksTextured.Get();

        MarkAllDirty();
    }

    // only the reflections, brick GI and the light debug views read the light
    const bool wantsGI = g_cvGlimmerBrickGI.Get();
    const bool wantsLight = g_cvGlimmerBricksReflections.Get() || wantsGI || m_framesSinceLightView < LightViewFrames;

    if (wantsLight != m_isLit)
    {
        m_isLit = wantsLight;

        if (m_isLit)
        {
            CreatePoolBuffers(PoolUse::Lit);

            // bricks only take cards while lit
            MarkAllDirty();

            m_hasRelitLight = false;
        }
        else
        {
            ReleasePoolBuffers(PoolUse::Lit);
        }
    }

    if (wantsGI != m_isGI)
    {
        m_isGI = wantsGI;

        if (m_isGI)
        {
            CreatePoolBuffers(PoolUse::GI);

            // every brick's slots are traced as it is built
            MarkAllDirty();

            m_hasRelitLight = false;
        }
        else
        {
            ReleasePoolBuffers(PoolUse::GI);

            m_hasRelitLight = false;
        }
    }

    MarkSceneChanges();

    if (m_occupancyGeneration != sceneInstances.GetGeneration())
    {
        MarkOccupiedChunks();

        m_occupancyGeneration = sceneInstances.GetGeneration();
    }

    // a chunk that ran out of room gets it from chunks farther away, or waits
    if (ReadBuildCounters(frame, viewerPosition))
    {
        hasWritten = true;
    }

    UpdateReach();

    // a chunk no span instance reaches is done without a build, and only cleared when it had bricks
    uint32 numClears = 0;

    for (Chunk& chunk : m_chunks)
    {
        if (!chunk.isDirty || (chunk.hasGeometry && IsInReach(chunk)) || !m_hasWindows[chunk.level])
        {
            continue;
        }

        if (chunk.hasBricks)
        {
            if (numClears == MaxEmptyChunkClearsPerFrame)
            {
                continue;
            }

            ClearChunk(frame, chunk);

            numClears++;
            hasWritten = true;
        }

        chunk.isBuilt = true;
        chunk.isDirty = false;
        chunk.isStarved = false;
    }

    // counted per frame, so a low frame rate gets more a frame to keep the same pace
    const uint32 maxBuilds = m_numSoloBuilds != 0
        ? 1u
        : uint32(MathUtil::Clamp(int32(float(g_cvGlimmerBricksChunksPerFrame.Get()) * MathUtil::Clamp(budgetScale, 1.0f, 8.0f)), 1, int32(GlimmerBrickMaxChunkBuildsPerFrame)));

    const Vec3f buildFocus = GetGlimmerWindowCenter(viewerPosition, lookAhead, Vec3f(ChunkDims) * LevelChunkSize(0));

    struct BuildCandidate
    {
        Chunk* chunk;
        float order;
    };

    Array<BuildCandidate, GlimmerTempAllocator> candidates;

    for (Chunk& chunk : m_chunks)
    {
        if (!chunk.isDirty || !chunk.hasGeometry || chunk.isStarved || !IsInReach(chunk))
        {
            continue;
        }

        candidates.PushBack(BuildCandidate { &chunk, BuildOrder(chunk, buildFocus) });
    }

    // a batch is of one level: the soonest chunk's
    FixedArray<Chunk*, GlimmerBrickMaxChunkBuildsPerFrame> batch;
    uint32 numBatched = 0;

    if (candidates.Any())
    {
        const BuildCandidate* soonest = std::min_element(candidates.Begin(), candidates.End(), [](const BuildCandidate& lhs, const BuildCandidate& rhs)
            {
                return lhs.order < rhs.order;
            });

        const uint32 batchLevel = soonest->chunk->level;

        BuildCandidate* levelEnd = std::partition(candidates.Begin(), candidates.End(), [batchLevel](const BuildCandidate& candidate)
            {
                return candidate.chunk->level == batchLevel;
            });

        const size_t numLevelCandidates = size_t(levelEnd - candidates.Begin());
        const size_t numTaken = MathUtil::Min(numLevelCandidates, size_t(maxBuilds));

        std::partial_sort(candidates.Begin(), candidates.Begin() + numTaken, levelEnd, [](const BuildCandidate& lhs, const BuildCandidate& rhs)
            {
                return lhs.order < rhs.order;
            });

        for (size_t candidateIndex = 0; candidateIndex < numTaken; candidateIndex++)
        {
            Chunk* chunk = candidates[candidateIndex].chunk;

            // taken for this batch
            chunk->isDirty = false;

            batch[numBatched++] = chunk;
        }
    }
    if (numBatched != 0)
    {
        BuildChunks(frame, batch.Data(), numBatched, technique);

        m_numSoloBuilds -= MathUtil::Min(m_numSoloBuilds, numBatched);

        // slots that saw through where these chunks were empty are traced again: a round of every brick from the last build on
        m_retraceBurst = 0;

        for (const Chunk& chunk : m_chunks)
        {
            m_retraceBurst += chunk.hasBricks ? chunk.numBricks : 0;
        }

        hasWritten = true;
    }

    const bool wasSettled = IsSettled();

    uint32 numDirtyBefore = 0;

    for (uint32 level = 0; level < GlimmerBrickLevels; level++)
    {
        numDirtyBefore += m_numDirtyChunks[level];
        m_numDirtyChunks[level] = 0;
    }

    uint32 numDirty = 0;

    for (const Chunk& chunk : m_chunks)
    {
        const bool isWaiting = chunk.isDirty && !chunk.isStarved && m_hasWindows[chunk.level];

        m_numDirtyChunks[chunk.level] += isWaiting ? 1 : 0;
        numDirty += isWaiting ? 1 : 0;
    }

    if (IsSettled() && !wasSettled)
    {
        HYP_LOG(Rendering, Debug, "Glimmer: bricks settled after {} frames", m_framesUnsettled);
    }

    if (numDirty == 0 && numDirtyBefore != 0)
    {
        const FixedArray<uint32, GlimmerBrickLevels> levelBricks = CountLevelBricks();

        HYP_LOG(Rendering, Info, "Glimmer: every brick level settled after {} frames; bricks by level {} {} {} {}; reach {} {} {} {} chunks", m_framesLevelsUnsettled, levelBricks[0], levelBricks[1], levelBricks[2], levelBricks[3],
            m_reachChunks[0], m_reachChunks[1], m_reachChunks[2], m_reachChunks[3]);

        m_wantsRebalance = WantsRebalancedPools();
    }

    m_framesUnsettled = IsSettled() ? 0 : m_framesUnsettled + 1;
    m_framesLevelsUnsettled = numDirty == 0 ? 0 : m_framesLevelsUnsettled + 1;

    m_framesSincePoolLog++;

    if ((m_numEvictions != 0 || m_numStarved != 0) && m_framesSincePoolLog >= PoolLogIntervalFrames)
    {
        HYP_LOG(Rendering, Debug, "Glimmer: brick pools full; {} chunks given up for nearer ones, {} left without room", m_numEvictions, m_numStarved);

        m_numEvictions = 0;
        m_numStarved = 0;
        m_framesSincePoolLog = 0;
    }

    if (hasWritten)
    {
        ListLiveBricks(frame);

        InsertBarriers(frame, false);
    }

    m_framesSinceLightView = MathUtil::Min(m_framesSinceLightView + 1, LightViewFrames);

    if (!m_isLit)
    {
        return;
    }

    // a change of sun or sky relights every brick in turn.
    // otherwise a trickle follows the bounce, and the sky's small drift, as they settle
    const float intensity = g_cvGlimmerIntensity.Get();

    if (!m_hasRelitLight || intensity != m_relitIntensity || GetGlimmerLightChange(m_relitLight, light) > RelightBurstChange)
    {
        m_relitLight = light;
        m_relitIntensity = intensity;
        m_hasRelitLight = true;

        m_relightBurst = 0;

        for (const Chunk& chunk : m_chunks)
        {
            m_relightBurst += chunk.hasBricks ? chunk.numBricks : 0;
        }
    }

    const uint32 numTrickleBricks = uint32(MathUtil::Max(g_cvGlimmerBrickGIRetraceBricks.Get(), 0));
    const uint32 numRetraceBricks = m_retraceBurst != 0 ? MathUtil::Max(RetraceBurstBricks, numTrickleBricks) : numTrickleBricks;

    m_retraceBurst -= MathUtil::Min(m_retraceBurst, numRetraceBricks);

    if (m_isGI && numRetraceBricks != 0)
    {
        TraceSlots(frame, m_retraceCursor, numRetraceBricks, false);

        m_retraceCursor += numRetraceBricks;
    }

    const uint32 numRelightBricks = MathUtil::Min(m_relightBurst != 0 ? RelightBricksPerFrame : RelightTrickleBricks, m_pools.x);

    m_relightBurst -= MathUtil::Min(m_relightBurst, numRelightBricks);

    if (numRelightBricks != 0)
    {
        LightBricks(frame, technique, m_relightCursor, numRelightBricks, false);

        m_relightCursor += numRelightBricks;
    }
}

void GlimmerBrickWorld::ListLiveBricks(Frame* frame)
{
    Dispatch(frame, BuildPass::Live, Box {}, 1, 0);
    Dispatch(frame, BuildPass::Live, Box {}, m_pools.x, 1);
}

void GlimmerBrickWorld::LightBricks(Frame* frame, const GlimmerTechnique* technique, uint32 firstBrick, uint32 numBricks, bool isBuildList)
{
    HYP_SCOPE;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickLight);

    CommandRecorder& cr = frame->cr;

    InsertBarriers(frame, false);

    const bool hasIndirect = technique && technique->IsReady();

    // past the bricks a shadow ray goes on through the heightfield, and slots that landed on the ground read its cache
    const bool hasHeightfield = m_surfaceCache && m_spanCache && m_spanCache->GetSpansBuffer().IsValid() && m_spanCache->GetHeightBoundsBuffer().IsValid()
        && m_surfaceCache->GetGroundImageView().IsValid();
    const bool hasGroundGI = m_isGI && hasHeightfield && m_groundGI && m_groundGI->IsReady();

    // a row of groups per brick: the build list's sized by the args pass, the live list's here
    const auto dispatchPass = [&](const StaticShaderPropertyId& modeProperty, uint32 level, uint32 threadsPerBrick, uint32 argsOffset)
    {
        GlimmerApplyShaderData applyShaderData {};
        applyShaderData.params = Vec4u(0u, hasIndirect ? 1u : 0u, 0u, 0u);
        applyShaderData.settings = Vec4f(MathUtil::Max(g_cvGlimmerIntensity.Get(), 0.0f), 0.0f, 0.0f, 0.0f);

        GlimmerBrickLightConstants constants {};
        constants.bricks = m_shaderData;
        constants.range = Vec4u(firstBrick, numBricks, isBuildList ? 1u : 0u, level);
        constants.params = Vec4f(ShadowBiasColumns, ShadowDistance, m_isGI ? 1.0f : 0.0f, GetGlimmerFoliageExtinction());
        constants.info = Vec4u(hasGroundGI ? 1u : 0u, hasHeightfield ? 1u : 0u, 0u, 0u);
        constants.light = m_light;

        if (hasHeightfield)
        {
            constants.ground = m_surfaceCache->GetGroundShaderData();
            constants.spans = m_spanCache->GetShaderData();
        }

        if (hasGroundGI)
        {
            constants.lit = m_groundGI->GetShaderData();
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

        if (m_isGI)
        {
            cr << InsertBarrier(m_slotsBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
            cr << InsertBarrier(m_arrivingBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }
        else
        {
            cr << InsertBarrier(m_placeholderBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickLight"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickLiveListBuffer"_sh, m_liveListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));

        uniformIndex = BindBuffers(cr, uniformIndex, GlimmerBrickBindings::Counters | GlimmerBrickBindings::Light | GlimmerBrickBindings::GI);
        uniformIndex = GlimmerTechnique::BindApplyResources(cr, uniformIndex, hasIndirect ? technique : nullptr);

        // HACK: any buffer or texture read only already will do in place of those not there, as the constants keep them from being read
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundGIBuffer"_sh, hasGroundGI ? m_groundGI->GetBuffer().Get() : m_indexBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, hasHeightfield ? m_surfaceCache->GetGroundImageView() : RI.placeholderData->GetImageView2D1x1R8Array());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, hasHeightfield ? m_spanCache->GetSpansBuffer().Get() : m_indexBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, hasHeightfield ? m_spanCache->GetHeightBoundsBuffer().Get() : m_indexBuffer.Get(), ShaderDataOffset(0, sizeof(float)));

        if (isBuildList)
        {
            cr << DispatchComputeIndirect(m_dispatchArgsBuffer.Get(), argsOffset * sizeof(uint32));
        }
        else
        {
            cr << DispatchCompute(Vec3u((threadsPerBrick + BrickGroupSize - 1) / BrickGroupSize, numBricks, 1));
        }
    };

    // what arrives at each tile from what its slots found as lit last, then the crossings lit with it
    if (m_isGI)
    {
        dispatchPass(s_propBrickLightModeGather, GlimmerBrickCardFirstMip, GlimmerBrickCards * GlimmerBrickGITiles, BrickArgsMips);
    }

    dispatchPass(s_propBrickLightModeLight, 0, BrickColumnsPerBrick, BrickArgsColumns);

    for (uint32 level = GlimmerBrickCardFirstMip; level < GlimmerBrickMips; level++)
    {
        const uint32 side = GlimmerBrickSide >> level;

        dispatchPass(s_propBrickLightModeMips, level, GlimmerBrickCards * side * side, BrickArgsMips + 3 * (level - GlimmerBrickCardFirstMip));
    }

    cr << InsertBarrier(m_radianceBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_crossingLightBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

    if (m_isGI)
    {
        cr << InsertBarrier(m_arrivingBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }
}

void GlimmerBrickWorld::TraceSlots(Frame* frame, uint32 firstBrick, uint32 numBricks, bool isBuildList)
{
    HYP_SCOPE;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickGITrace);

    CommandRecorder& cr = frame->cr;

    InsertBarriers(frame, false);

    const bool hasHeightfield = m_surfaceCache && m_spanCache && m_spanCache->GetSpansBuffer().IsValid() && m_spanCache->GetHeightBoundsBuffer().IsValid()
        && m_surfaceCache->GetGroundImageView().IsValid() && m_surfaceCache->GetGroundAlbedoImageView().IsValid();

    GlimmerBrickGITraceConstants constants {};
    constants.bricks = m_shaderData;
    const bool hasGroundGI = hasHeightfield && m_groundGI && m_groundGI->IsReady();

    constants.range = Vec4u(firstBrick, numBricks, isBuildList ? 1u : 0u, hasHeightfield ? (hasGroundGI ? 2u : 1u) : 0u);
    constants.params = Vec4f(GIRayBiasColumns, GlimmerMaxRayDistance, GetGlimmerFoliageExtinction(), GlimmerDefaultGroundAlbedo);

    if (hasHeightfield)
    {
        constants.ground = m_surfaceCache->GetGroundShaderData();
        constants.spans = m_spanCache->GetShaderData();
    }

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << InsertBarrier(m_slotsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickGITrace")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickLiveListBuffer"_sh, m_liveListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickSlotsBuffer"_sh, m_slotsBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));

    uniformIndex = BindBuffers(cr, uniformIndex, GlimmerBrickBindings::Counters);

    if (hasHeightfield)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, m_surfaceCache->GetGroundImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, m_surfaceCache->GetGroundAlbedoImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, m_spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, m_spanCache->GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));
    }
    else
    {
        cr << InsertBarrier(m_placeholderBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, RI.placeholderData->GetImageView2D1x1R8Array());
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, RI.placeholderData->GetImageView2D1x1R8Array());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, m_placeholderBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, m_placeholderBuffer.Get(), ShaderDataOffset(0, sizeof(float)));
    }

    // a row of groups per brick, as the light's columns
    if (isBuildList)
    {
        cr << DispatchComputeIndirect(m_dispatchArgsBuffer.Get(), BrickArgsColumns * sizeof(uint32));
    }
    else
    {
        cr << DispatchCompute(Vec3u((BrickColumnsPerBrick + BrickGroupSize - 1) / BrickGroupSize, numBricks, 1));
    }

    cr << InsertBarrier(m_slotsBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
}

Vec2f GlimmerBrickWorld::GetLodReach() const
{
    return m_triangleCache ? GlimmerSceneInstances::GetSolidLodError(m_triangleCache->GetLodErrorScale()) : Vec2f::Zero();
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

    const Vec3i originChunk = m_originChunks[0];

    outOrigin = Vec4i(originChunk.x, originChunk.y, originChunk.z, 1);

    for (uint32 slot = 0; slot < GlimmerBrickChunks; slot++)
    {
        const Chunk& chunk = m_chunks[slot];

        if ((chunk.isBuilt && !chunk.isDirty) || chunk.isStarved)
        {
            continue;
        }

        const Vec3i local = chunk.coord - originChunk;

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

    if (debugView == GlimmerBrickDebugView::Light || debugView == GlimmerBrickDebugView::LightCoarse)
    {
        m_framesSinceLightView = 0;
    }

    if (!IsReady() || !context.sceneInstances || !context.sceneInstances->IsReady() || !context.triangleCache || !context.triangleCache->IsReady())
    {
        return false;
    }

    // the light views wait for the light, which they ask for above
    if ((debugView == GlimmerBrickDebugView::Light || debugView == GlimmerBrickDebugView::LightCoarse) && !m_isLit)
    {
        return false;
    }

    if (debugView == GlimmerBrickDebugView::Arriving && !m_isGI)
    {
        return false;
    }


    Framebuffer* gbufferFramebuffer = context.gbufferFramebuffer;
    const Vec2u extent = context.extent;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickDebug);

    GlimmerBrickDebugConstants constants {};
    constants.dimensionsMode = Vec4u(extent.x, extent.y, uint32(debugView), 0);
    const Vec2f lodReach = GetLodReach();

    constants.params = Vec4f(10000.0f, lodReach.x, lodReach.y, 0.0f);
    constants.info = Vec4u(m_pools, 0u);
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
    cr << InsertBarrier((m_isLit ? m_radianceBuffer : m_placeholderBuffer).Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier((m_isLit ? m_crossingLightBuffer : m_placeholderBuffer).Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier((m_isGI ? m_arrivingBuffer : m_placeholderBuffer).Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickDebug")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "OutImage"_sh, context.outputImageView);
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(context.view->GetCamera()));

    uniformIndex = BindBuffers(cr, uniformIndex, GlimmerBrickBindings::Counters | GlimmerBrickBindings::Light);

    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickArrivingBuffer"_sh, (m_isGI ? m_arrivingBuffer : m_placeholderBuffer).Get(), ShaderDataOffset(0, sizeof(uint32)));

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });

    return true;
}

#pragma endregion GlimmerBrickWorld

} // namespace Hyperion
