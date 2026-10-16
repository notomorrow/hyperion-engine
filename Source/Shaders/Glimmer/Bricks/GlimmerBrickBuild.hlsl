#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Aabb.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../GlimmerCommon.hlsli"

//////////////////////////////////////////////////////
/// chunks are built a batch at a time, each in a slot.
///     - BEGIN readies the slots, BIN puts each triangle on the list of every brick of its axis it touches,
///     - ALLOC takes the bricks and lists them, ARGS sizes the passes that follow by that list
///     - GATHER makes crossings of a brick's list, a thread per column
///     - LIVE lists the bricks in use for the relight: options.y = 0 empties the list, 1 fills it, a thread per pool brick
///     - GROW hands out what the pools gained, boxMin.xyz being the bricks, pages and cards they had
//////////////////////////////////////////////////////

PERMUTE(MODE, INIT, BEGIN, CLEAR, BIN, ALLOC, ARGS, COUNT, PREFIX, GATHER, TILES, LIVE, GROW)

DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickTableBuffer) RWStructuredBuffer<uint> glimmerBrickTable;
DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickIndexBuffer) RWStructuredBuffer<uint> glimmerBrickIndex;
// @TODO Make RWByteAddressBuffer, and use Load2 for loading brick crossings ? Or I guess RWStructuredBuffer<uint2> ?
DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickCrossingsBuffer) RWStructuredBuffer<uint> glimmerBrickCrossings; 
DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickMaterialBuffer) RWStructuredBuffer<uint> glimmerBrickMaterial;
DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickCountersBuffer) RWStructuredBuffer<uint> glimmerBrickCounters;
DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickFreeListBuffer) RWStructuredBuffer<uint> glimmerBrickFreeList;
DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickPageFreeListBuffer) RWStructuredBuffer<uint> glimmerBrickPageFreeList;
DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickCardFreeListBuffer) RWStructuredBuffer<uint> glimmerBrickCardFreeList;
DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickListNodesBuffer) RWStructuredBuffer<uint> glimmerBrickListNodes;
DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickLiveListBuffer) RWStructuredBuffer<uint> glimmerBrickLiveList;

DECLARE_UAV(GlimmerBrickBuild, GlimmerBrickDispatchArgsBuffer) RWStructuredBuffer<uint> glimmerBrickDispatchArgs;

#include "GlimmerBricks.hlsli"

struct GlimmerBrickBuildConstants
{
    GlimmerBrickParams bricks;
    int4 boxMin;                        // xyz = absolute cell, w = pool cards
    int4 boxExtent;                     // xyz = cells, w = pool pages
    uint4 counts;                       // x = span chunks, y = groups along x, z = pool bricks, w = list node capacity
    uint4 options;                      // x = 1 to sample the textures, y = the LIVE step, z = chunks in the batch, w = 1 to give bricks cards
    int4 chunkMin[MAX_BATCH_CHUNKS];    // xyz = absolute first cell of each slot's chunk
};

DECLARE_BUFFER_DYNAMIC(GlimmerBrickBuild, CBuffer) cbuffer CBuffer
{
    GlimmerBrickBuildConstants constants;
};

DECLARE_SRV(GlimmerBrickBuild, SpanInstancesBuffer) StructuredBuffer<GlimmerSpanInstance> spanInstances;
DECLARE_SRV(GlimmerBrickBuild, SpanChunksBuffer) StructuredBuffer<GlimmerSpanChunk> spanChunks;
DECLARE_SRV(GlimmerBrickBuild, GlimmerTrianglesBuffer) StructuredBuffer<uint> glimmerTriangles;
DECLARE_SRV(GlimmerBrickBuild, GlimmerTriangleTexcoordsBuffer) StructuredBuffer<uint> glimmerTriangleTexcoords;
DECLARE_SRV(GlimmerBrickBuild, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerBrickBuild, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

#include "../GlimmerMaterial.hlsli"

#define GROUP_SIZE 64

#define LIST_NODE_WORDS 3u
#define MAX_LIST_STEPS 65536u

#define TILES_PER_BRICK (GLIMMER_BRICK_TILES_PER_SIDE * GLIMMER_BRICK_TILES_PER_SIDE)

/// the indirect dispatches, in groups
///  must match GlimmerBrickWorld.cpp
#define ARGS_COLUMNS 0u
#define ARGS_TILES 3u
#define ARGS_PREFIX 6u
#define ARGS_MIPS 9u

uint SlotCounter(uint slot, uint counter)
{
    return SLOT_COUNTERS_BASE + slot * uint(SLOT_COUNTERS) + counter;
}

int3 SlotCell(uint slot, uint slotBrick)
{
    const uint cellIndex = slotBrick / 3u;
    const uint side = uint(GLIMMER_BRICK_CHUNK_CELLS);

    return constants.chunkMin[slot].xyz + int3(cellIndex % side, (cellIndex / side) % side, cellIndex / (side * side));
}

uint SlotBrick(int3 local, uint axis)
{
    const uint side = uint(GLIMMER_BRICK_CHUNK_CELLS);

    return ((uint(local.z) * side + uint(local.y)) * side + uint(local.x)) * 3u + axis;
}

void Append(RWStructuredBuffer<uint> list, uint counter, uint value)
{
    uint at;
    InterlockedAdd(glimmerBrickCounters[counter], 1u, at);

    list[at] = value;
}

// takes one from a free list for a slot, or counts it as short of one
bool PopFree(RWStructuredBuffer<uint> freeList, uint counter, uint slot, uint failedSlotCounter, uint takenSlotCounter, out uint outValue)
{
    uint numFree;
    InterlockedAdd(glimmerBrickCounters[counter], 0xFFFFFFFFu, numFree);

    outValue = GLIMMER_BRICK_NONE;

    if (int(numFree) <= 0)
    {
        InterlockedAdd(glimmerBrickCounters[counter], 1u);
        InterlockedAdd(glimmerBrickCounters[COUNTER_FAILED_BRICKS], 1u);
        InterlockedAdd(glimmerBrickCounters[SlotCounter(slot, failedSlotCounter)], 1u);

        return false;
    }

    InterlockedAdd(glimmerBrickCounters[SlotCounter(slot, takenSlotCounter)], 1u);

    outValue = freeList[numFree - 1u];

    return true;
}

void WriteArgs(uint args, uint groupsX, uint groupsY)
{
    glimmerBrickDispatchArgs[args] = groupsX;
    glimmerBrickDispatchArgs[args + 1u] = groupsY;
    glimmerBrickDispatchArgs[args + 2u] = 1u;
}

// a column's crossings stay within one page: where its first goes
uint PlaceColumn(inout uint cursor, uint count)
{
    const uint pageMask = uint(GLIMMER_BRICK_PAGE_CROSSINGS) - 1u;

    if ((cursor & pageMask) + count > uint(GLIMMER_BRICK_PAGE_CROSSINGS))
    {
        cursor = (cursor + pageMask) & ~pageMask;
    }

    const uint at = cursor;

    cursor += count;

    return at;
}

// crossings within this many column widths in depth, with normals no further apart than SAME_FACING, are one surface
#define SAME_DEPTH 0.5
#define SAME_FACING 0.8
#define FLAT_AGREEMENT 0.9999

// columns' worth of surface a brick needs to be built
#define MIN_BRICK_COLUMNS 48.0
#define AREA_SCALE 4.0

uint BoxCells()
{
    return uint(constants.boxExtent.x * constants.boxExtent.y * constants.boxExtent.z);
}

int3 BoxCell(uint cellIndex)
{
    const uint3 extent = uint3(constants.boxExtent.xyz);

    return constants.boxMin.xyz + int3(cellIndex % extent.x, (cellIndex / extent.x) % extent.y, cellIndex / (extent.x * extent.y));
}

float3 TransformPoint(GlimmerSpanInstance instance, float3 position)
{
    const float4 homogeneous = float4(position, 1.0);

    return float3(dot(instance.objectToWorld0, homogeneous), dot(instance.objectToWorld1, homogeneous), dot(instance.objectToWorld2, homogeneous));
}

struct WorldTriangle
{
    float3 p0;
    float3 p1;
    float3 p2;
    float3 normal; // unit length, facing the way the surface does
    bool isValid;
};

WorldTriangle LoadWorldTriangle(GlimmerSpanInstance instance, uint localTriangle)
{
    const GlimmerTriangle glimmerTriangle = LOAD_GLIMMER_TRIANGLE(glimmerTriangles, instance.data.x + localTriangle);

    WorldTriangle worldTriangle;
    worldTriangle.p0 = TransformPoint(instance, glimmerTriangle.position0);
    worldTriangle.p1 = TransformPoint(instance, glimmerTriangle.position1);
    worldTriangle.p2 = TransformPoint(instance, glimmerTriangle.position2);

    const float3 normal = cross(worldTriangle.p1 - worldTriangle.p0, worldTriangle.p2 - worldTriangle.p0);
    const float doubleArea = length(normal);

    worldTriangle.isValid = doubleArea > 1e-10;
    worldTriangle.normal = normal / max(doubleArea, 1e-20) * ((instance.data.w & GLIMMER_INSTANCE_FLAG_MIRRORED) != 0u ? -1.0 : 1.0);

    return worldTriangle;
}

#if defined(MODE_BIN)

void BinTriangle(GlimmerSpanInstance instance, uint spanInstanceIndex, uint localTriangle, uint slotMask)
{
    const WorldTriangle worldTriangle = LoadWorldTriangle(instance, localTriangle);

    if (!worldTriangle.isValid)
    {
        return;
    }

    const uint axis = GlimmerBrickAxis(worldTriangle.normal);

    const float cellSize = constants.bricks.params.x;
    const float margin = GlimmerBrickDepthMargin(constants.bricks);

    const float3 triangleMin = min(worldTriangle.p0, min(worldTriangle.p1, worldTriangle.p2));
    const float3 triangleMax = max(worldTriangle.p0, max(worldTriangle.p1, worldTriangle.p2));

    // the margin is along the brick's axis only: across it a triangle is in the cells it is over
    const float3 reach = GlimmerBrickPoint(axis, (float2)0.0, margin);

    const int3 triangleCellMin = int3(floor((triangleMin - reach) * constants.bricks.params.y));
    const int3 triangleCellMax = int3(floor((triangleMax + reach) * constants.bricks.params.y));

    const float3 halfExtent = (float3)(0.5 * cellSize) + reach;

    // what the triangle covers across a brick, in columns: no more than its box does of the cell
    const float3 doubleArea = cross(worldTriangle.p1 - worldTriangle.p0, worldTriangle.p2 - worldTriangle.p0);
    const float projectedColumns = 0.5 * abs(GlimmerBrickAlong(axis, doubleArea)) * constants.bricks.params.w * constants.bricks.params.w;
    const float2 triangleMinAcross = GlimmerBrickAcross(axis, triangleMin) * constants.bricks.params.w;
    const float2 triangleMaxAcross = GlimmerBrickAcross(axis, triangleMax) * constants.bricks.params.w;

    for (uint slots = slotMask; slots != 0u; slots &= slots - 1u)
    {
        const uint slot = firstbitlow(slots);
        const int3 chunkMin = constants.chunkMin[slot].xyz;

        const int3 cellMin = max(triangleCellMin, chunkMin);
        const int3 cellMax = min(triangleCellMax, chunkMin + int(GLIMMER_BRICK_CHUNK_CELLS) - 1);

        if (any(cellMax < cellMin))
        {
            continue;
        }

        for (int z = cellMin.z; z <= cellMax.z; z++)
        {
            for (int y = cellMin.y; y <= cellMax.y; y++)
            {
                for (int x = cellMin.x; x <= cellMax.x; x++)
                {
                    const int3 cell = int3(x, y, z);
                    const float3 center = (float3(cell) + 0.5) * cellSize;

                    if (!TriangleOverlapsBox(worldTriangle.p0, worldTriangle.p1, worldTriangle.p2, worldTriangle.normal, center, halfExtent))
                    {
                        continue;
                    }

                    uint node;
                    InterlockedAdd(glimmerBrickCounters[COUNTER_LIST_NODES], 1u, node);

                    if (node >= constants.counts.w)
                    {
                        InterlockedAdd(glimmerBrickCounters[COUNTER_DROPPED_NODES], 1u);
                        InterlockedAdd(glimmerBrickCounters[COUNTER_BATCH_DROPPED_NODES], 1u);

                        return;
                    }

                    glimmerBrickListNodes[node * LIST_NODE_WORDS] = spanInstanceIndex;
                    glimmerBrickListNodes[node * LIST_NODE_WORDS + 1u] = localTriangle;

                    uint next;
                    InterlockedExchange(glimmerBrickCounters[LIST_HEADS_BASE + slot * BOX_BRICKS + SlotBrick(cell - chunkMin, axis)], node, next);

                    // the table holds no brick while its box is built, so it adds up the area: one less than it, from GLIMMER_BRICK_NONE
                    const float2 cellMinAcross = GlimmerBrickAcross(axis, float3(cell)) * float(GLIMMER_BRICK_SIDE);
                    const float2 overlap = max(min(triangleMaxAcross, cellMinAcross + float(GLIMMER_BRICK_SIDE)) - max(triangleMinAcross, cellMinAcross), 0.0);
                    const uint coveredArea = uint(min(projectedColumns, overlap.x * overlap.y) * AREA_SCALE + 0.5);

                    InterlockedAdd(glimmerBrickTable[GlimmerBrickTableIndex(cell, axis)], coveredArea);

                    glimmerBrickListNodes[node * LIST_NODE_WORDS + 2u] = next;
                }
            }
        }
    }
}

#endif

#if defined(MODE_GATHER) || defined(MODE_COUNT)

struct Gathered
{
    float depth;
    float3 normal;
    float3 color;
    float2 finish;
    float weight;
    uint mask;
    uint material;
    float most;
    bool isDoubleSided;
};

// metalness and roughness
float2 MaterialFinish(uint materialIndex)
{
    if (materialIndex == 0xFFFFFFFFu)
    {
        return float2(0.0, 1.0);
    }

    const Material material = materials[materialIndex];

    return float2(GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS), GET_MATERIAL_PARAM(material, MATERIAL_PARAM_ROUGHNESS));
}

#if defined(MODE_GATHER) && defined(HYP_FEATURES_BINDLESS_TEXTURES)

#define GLIMMER_MATERIAL_TEXTURE(material, name) textures[NonUniformResourceIndex((material).texture_indices[MATERIAL_TEXTURE_##name / 4][MATERIAL_TEXTURE_##name % 4])]

// at the mip a texel of which is a column across, from how far the UV moves a column across each way
float4 SampleAcrossColumn(Texture2D tex, float2 uv, float2 uvAcrossX, float2 uvAcrossY)
{
    uint width;
    uint height;
    uint levels;
    tex.GetDimensions(0, width, height, levels);

    const float2 size = float2(width, height);
    const float footprint = max(length(uvAcrossX * size), length(uvAcrossY * size));

    return tex.SampleLevel(glimmerMaterialSampler, uv, clamp(log2(max(footprint, 1e-6)), 0.0, float(levels)));
}

// color, metalness and roughness from the textures where the triangle is nearest the middle of the parts it covers
void SampleTriangleSurface(GlimmerSpanInstance instance, uint localTriangle, float2 a, float2 b, float2 c, uint mask, inout float3 color, inout float2 finish)
{
    const uint texcoordBase = instance.lightmap.w;
    const uint materialIndex = instance.data.z;

    if (constants.options.x == 0u || texcoordBase == 0xFFFFFFFFu || materialIndex == 0xFFFFFFFFu)
    {
        return;
    }

    const Material material = materials[materialIndex];

    const uint sampledMaps = (1u << MATERIAL_TEXTURE_DiffuseMap) | (1u << MATERIAL_TEXTURE_MetalnessMap) | (1u << MATERIAL_TEXTURE_RoughnessMap);

    if ((material.textureUsage & sampledMaps) == 0u)
    {
        return;
    }

    const float2 ab = b - a;
    const float2 ac = c - a;
    const float det = ab.x * ac.y - ab.y * ac.x;

    if (abs(det) < 1e-12)
    {
        return;
    }

    const uint word = (texcoordBase + localTriangle) * 4u;
    const uint packedBase = glimmerTriangleTexcoords[word];
    const float2 base = float2(float(int(packedBase << 16u) >> 16), float(int(packedBase) >> 16));

    float2 uvs[3];

    for (uint corner = 0; corner < 3u; corner++)
    {
        const uint packed = glimmerTriangleTexcoords[word + 1u + corner];
        const float2 uv = base + f16tof32(uint2(packed & 0xFFFFu, packed >> 16u));

        // as the geometry pass has them
        uvs[corner] = float2(uv.x, 1.0 - uv.y) * material.uv_scale;
    }

    const float2 at = GlimmerBrickMaskMiddle(mask);

    float2 weights = float2((at.x - a.x) * ac.y - (at.y - a.y) * ac.x, ab.x * (at.y - a.y) - ab.y * (at.x - a.x)) / det;
    weights = max(weights, (float2)0.0);
    weights /= max(weights.x + weights.y, 1.0);

    const float2 uvAlongAB = uvs[1] - uvs[0];
    const float2 uvAlongAC = uvs[2] - uvs[0];

    const float2 uv = uvs[0] + weights.x * uvAlongAB + weights.y * uvAlongAC;
    const float2 uvAcrossX = (uvAlongAB * ac.y - uvAlongAC * ab.y) / det;
    const float2 uvAcrossY = (uvAlongAC * ab.x - uvAlongAB * ac.x) / det;

    if (HAS_TEXTURE(material, DiffuseMap))
    {
        color = material.albedo.rgb * SampleAcrossColumn(GLIMMER_MATERIAL_TEXTURE(material, DiffuseMap), uv, uvAcrossX, uvAcrossY).rgb;
    }

    if (HAS_TEXTURE(material, MetalnessMap))
    {
        finish.x = SelectMaterialChannel(SampleAcrossColumn(GLIMMER_MATERIAL_TEXTURE(material, MetalnessMap), uv, uvAcrossX, uvAcrossY), GET_MATERIAL_METALNESS_CHANNEL(material));
    }

    if (HAS_TEXTURE(material, RoughnessMap))
    {
        finish.y = SelectMaterialChannel(SampleAcrossColumn(GLIMMER_MATERIAL_TEXTURE(material, RoughnessMap), uv, uvAcrossX, uvAcrossY), GET_MATERIAL_ROUGHNESS_CHANNEL(material));
    }
}

#endif

// which sixteenths of a column a triangle has anything in, from its corners across the column in columns
uint TriangleColumnMask(float2 a, float2 b, float2 c)
{
    const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);

    if (abs(area) < 1e-12)
    {
        return 0u;
    }

    const float turn = area > 0.0 ? 1.0 : -1.0;
    const float2 corners[3] = { a, b, c };
    const float2 low = min(a, min(b, c));
    const float2 high = max(a, max(b, c));

    uint mask = 0u;

    for (uint part = 0; part < 16u; part++)
    {
        const float2 partLow = float2(float(part & 3u), float(part >> 2u)) * 0.25;

        if (any(partLow > high) || any(partLow + 0.25 < low))
        {
            continue;
        }

        const float2 middle = partLow + 0.125;

        bool isInside = true;

        [unroll]
        for (uint edgeIndex = 0; edgeIndex < 3u; edgeIndex++)
        {
            const float2 from = corners[edgeIndex];
            const float2 along = corners[(edgeIndex + 1u) % 3u] - from;

            const float within = turn * (along.x * (middle.y - from.y) - along.y * (middle.x - from.x));
            const float reach = 0.125 * (abs(along.x) + abs(along.y));

            isInside = isInside && within + reach >= 0.0;
        }

        if (isInside)
        {
            mask |= 1u << part;
        }
    }

    return mask;
}

void GatherColumn(uint brick, uint axis, int3 cell, uint listHead, int2 column)
{
    const float width = constants.bricks.params.z;
    const float cellSize = constants.bricks.params.x;

    const float3 columnLow = GlimmerBrickCellLow(constants.bricks, cell) + GlimmerBrickPoint(axis, float2(column) * width, 0.0);
    const float3 middle = columnLow + GlimmerBrickPoint(axis, (float2)(0.5 * width), 0.0);

    Gathered found[GLIMMER_BRICK_MAX_COLUMN_CROSSINGS];
    uint count = 0u;

    for (uint clearIndex = 0; clearIndex < GLIMMER_BRICK_MAX_COLUMN_CROSSINGS; clearIndex++)
    {
        found[clearIndex] = (Gathered)0;
    }

    uint node = listHead;

    [loop]
    for (uint listStep = 0; listStep < MAX_LIST_STEPS && node != GLIMMER_BRICK_NONE; listStep++)
    {
        const uint spanInstanceIndex = glimmerBrickListNodes[node * LIST_NODE_WORDS];
        const uint localTriangle = glimmerBrickListNodes[node * LIST_NODE_WORDS + 1u];

        node = glimmerBrickListNodes[node * LIST_NODE_WORDS + 2u];

        const GlimmerSpanInstance instance = spanInstances[spanInstanceIndex];
        const WorldTriangle worldTriangle = LoadWorldTriangle(instance, localTriangle);

        const float2 a = GlimmerBrickAcross(axis, worldTriangle.p0 - columnLow) * constants.bricks.params.w;
        const float2 b = GlimmerBrickAcross(axis, worldTriangle.p1 - columnLow) * constants.bricks.params.w;
        const float2 c = GlimmerBrickAcross(axis, worldTriangle.p2 - columnLow) * constants.bricks.params.w;

        if (any(min(a, min(b, c)) > 1.0) || any(max(a, max(b, c)) < 0.0))
        {
            continue;
        }

        const float3 normal = worldTriangle.normal;
        const float depth = dot(normal, worldTriangle.p0 - middle) / GlimmerBrickAlong(axis, normal);

        if (depth < -width || depth >= cellSize + width)
        {
            continue;
        }

        const uint mask = TriangleColumnMask(a, b, c);

        if (mask == 0u)
        {
            continue;
        }

        const float weight = float(countbits(mask));

        uint slot = GLIMMER_BRICK_NONE;

        for (uint foundIndex = 0; foundIndex < count && slot == GLIMMER_BRICK_NONE; foundIndex++)
        {
            if (abs(found[foundIndex].depth / found[foundIndex].weight - depth) < SAME_DEPTH * width
                && dot(normalize(found[foundIndex].normal), normal) > SAME_FACING)
            {
                slot = foundIndex;
            }
        }

        if (slot == GLIMMER_BRICK_NONE)
        {
            if (count == GLIMMER_BRICK_MAX_COLUMN_CROSSINGS)
            {
                continue;
            }

            slot = count++;
        }

        found[slot].depth += depth * weight;
        found[slot].normal += normal * weight;
        found[slot].weight += weight;
        found[slot].mask |= mask;

#if defined(MODE_GATHER)
        if (constants.options.x != 0u)
        {
            float3 color = GlimmerGetMaterialAverageAlbedo(instance.data.z);
            float2 finish = MaterialFinish(instance.data.z);

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
            SampleTriangleSurface(instance, localTriangle, a, b, c, mask, color, finish);
#endif

            found[slot].color += color * weight;
            found[slot].finish += finish * weight;
        }
#endif

        if (weight > found[slot].most)
        {
            found[slot].most = weight;
            found[slot].material = instance.data.z;
            found[slot].isDoubleSided = (instance.data.w & GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED) != 0u;
        }
    }

#if defined(MODE_COUNT)
    glimmerBrickIndex[GlimmerBrickColumnIndex(brick, column)] = count << 24u;
#else
    uint order[GLIMMER_BRICK_MAX_COLUMN_CROSSINGS];
    float depths[GLIMMER_BRICK_MAX_COLUMN_CROSSINGS];

    for (uint orderIndex = 0; orderIndex < GLIMMER_BRICK_MAX_COLUMN_CROSSINGS; orderIndex++)
    {
        order[orderIndex] = orderIndex;
        depths[orderIndex] = orderIndex < count ? found[orderIndex].depth / found[orderIndex].weight : 0.0;
    }

    // nearest first
    for (uint sortIndex = 1; sortIndex < count; sortIndex++)
    {
        const uint moved = order[sortIndex];

        uint insertAt = sortIndex;

        while (insertAt > 0u && depths[order[insertAt - 1u]] > depths[moved])
        {
            order[insertAt] = order[insertAt - 1u];
            insertAt--;
        }

        order[insertAt] = moved;
    }

    // where the prefix pass put this column, or nowhere when the brick got no pages
    uint first;
    count = min(count, GlimmerLoadBrickColumn(brick, column, first));

    for (uint writeIndex = 0; writeIndex < count; writeIndex++)
    {
        const Gathered crossing = found[order[writeIndex]];

        const float3 normal = normalize(crossing.normal);
        const float agreement = length(crossing.normal) / crossing.weight;

        uint flags = 0u;

        if (agreement < FLAT_AGREEMENT)
        {
            flags |= GLIMMER_BRICK_FLAG_CURVED;
        }

        if (crossing.isDoubleSided)
        {
            flags |= GLIMMER_BRICK_FLAG_DOUBLE_SIDED;
        }

        float3 color = float3(0.5, 0.5, 0.5);
        float2 finish = MaterialFinish(crossing.material);

        if (constants.options.x != 0u)
        {
            color = crossing.color / crossing.weight;
            finish = crossing.finish / crossing.weight;
        }
        else if (crossing.material != 0xFFFFFFFFu)
        {
            color = GlimmerGetMaterialAverageAlbedo(crossing.material);
        }

        const uint2 packedFinish = uint2(saturate(finish) * 63.0 + 0.5);
        const uint crossingIndex = first + writeIndex;

        glimmerBrickCrossings[2u * crossingIndex] = GlimmerBrickPackDepth(constants.bricks, depths[order[writeIndex]]) | (GlimmerBrickPackNormal(normal) << 16u);
        glimmerBrickCrossings[2u * crossingIndex + 1u] = crossing.mask | (GlimmerBrickPackColor(color) << 16u);

        glimmerBrickMaterial[crossingIndex] = (crossing.material & 0xFFFFu) | (packedFinish.x << 16u) | (packedFinish.y << 22u) | (flags << 29u);

        InterlockedOr(glimmerBrickIndex[GlimmerBrickWord(brick, GLIMMER_BRICK_INDEX_CARD_MASK)], 1u << GlimmerBrickCardSlot(constants.bricks, axis, normal, depths[order[writeIndex]]));
    }

#endif
}

#endif

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint flatGroupIndex = groupId.y * constants.counts.y + groupId.x;
    const uint threadIndex = flatGroupIndex * GROUP_SIZE + groupIndex;

#if defined(MODE_INIT)
    if (threadIndex < uint(GLIMMER_BRICK_GRID_XZ * GLIMMER_BRICK_GRID_Y * GLIMMER_BRICK_GRID_XZ * 3))
    {
        glimmerBrickTable[threadIndex] = GLIMMER_BRICK_NONE;
    }

    if (threadIndex < constants.counts.z)
    {
        glimmerBrickFreeList[threadIndex] = constants.counts.z - 1u - threadIndex;
        glimmerBrickIndex[GlimmerBrickWord(threadIndex, GLIMMER_BRICK_INDEX_AXIS)] = GLIMMER_BRICK_NONE;
    }

    if (threadIndex < uint(constants.boxMin.w))
    {
        glimmerBrickCardFreeList[threadIndex] = uint(constants.boxMin.w) - 1u - threadIndex;
    }

    if (threadIndex < uint(constants.boxExtent.w))
    {
        glimmerBrickPageFreeList[threadIndex] = uint(constants.boxExtent.w) - 1u - threadIndex;
    }

    if (threadIndex < NUM_COUNTERS)
    {
        glimmerBrickCounters[threadIndex] = threadIndex == COUNTER_FREE_BRICKS ? constants.counts.z : (threadIndex == COUNTER_FREE_PAGES ? uint(constants.boxExtent.w) : (threadIndex == COUNTER_FREE_CARDS ? uint(constants.boxMin.w) : 0u));
    }
#elif defined(MODE_BEGIN)
    if (threadIndex == 0u)
    {
        glimmerBrickCounters[COUNTER_LIST_NODES] = 0u;
        glimmerBrickCounters[COUNTER_BUILD_BRICKS] = 0u;
        glimmerBrickCounters[COUNTER_BATCH_DROPPED_NODES] = 0u;
    }

    if (threadIndex < constants.options.z * uint(SLOT_COUNTERS))
    {
        glimmerBrickCounters[SLOT_COUNTERS_BASE + threadIndex] = 0u;
    }

    if (threadIndex < constants.options.z * BOX_BRICKS)
    {
        glimmerBrickCounters[LIST_HEADS_BASE + threadIndex] = GLIMMER_BRICK_NONE;
    }
#elif defined(MODE_CLEAR)
    if (threadIndex >= BoxCells() * 3u)
    {
        return;
    }

    const uint tableIndex = GlimmerBrickTableIndex(BoxCell(threadIndex / 3u), threadIndex % 3u);
    const uint brick = glimmerBrickTable[tableIndex];

    if (brick != GLIMMER_BRICK_NONE)
    {
        Append(glimmerBrickFreeList, COUNTER_FREE_BRICKS, brick);

        glimmerBrickTable[tableIndex] = GLIMMER_BRICK_NONE;

        const uint header = GlimmerBrickWord(brick, 0u);
        const uint numPages = min(glimmerBrickIndex[header], uint(GLIMMER_BRICK_MAX_PAGES));

        uint pageSlot;
        InterlockedAdd(glimmerBrickCounters[COUNTER_FREE_PAGES], numPages, pageSlot);

        for (uint pageIndex = 0; pageIndex < numPages; pageIndex++)
        {
            glimmerBrickPageFreeList[pageSlot + pageIndex] = glimmerBrickIndex[header + 1u + pageIndex];
        }

        glimmerBrickIndex[header] = 0u;
        glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_AXIS)] = GLIMMER_BRICK_NONE;

        for (uint cardSlot = 0; cardSlot < uint(GLIMMER_BRICK_CARDS); cardSlot++)
        {
            const uint card = glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CARDS) + cardSlot];

            if (card != GLIMMER_BRICK_NONE)
            {
                Append(glimmerBrickCardFreeList, COUNTER_FREE_CARDS, card);

                glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CARDS) + cardSlot] = GLIMMER_BRICK_NONE;
            }
        }
    }
#elif defined(MODE_BIN)
    // a group per span chunk, for the slots whose chunks it reaches
    const uint chunkIndex = flatGroupIndex;

    if (chunkIndex >= constants.counts.x)
    {
        return;
    }

    const GlimmerSpanChunk chunk = spanChunks[chunkIndex];

    const float margin = GlimmerBrickDepthMargin(constants.bricks);
    const float chunkSize = float(GLIMMER_BRICK_CHUNK_CELLS) * constants.bricks.params.x;

    uint slotMask = 0u;

    for (uint slot = 0; slot < constants.options.z; slot++)
    {
        const float3 boxMinPosition = float3(constants.chunkMin[slot].xyz) * constants.bricks.params.x - margin;
        const float3 boxMaxPosition = boxMinPosition + chunkSize + 2.0 * margin;

        if (all(chunk.boundsMax.xyz >= boxMinPosition) && all(chunk.boundsMin.xyz <= boxMaxPosition))
        {
            slotMask |= 1u << slot;
        }
    }

    if (slotMask == 0u)
    {
        return;
    }

    const uint spanInstanceIndex = asuint(chunk.boundsMin.w);
    const GlimmerSpanInstance instance = spanInstances[spanInstanceIndex];

    if ((instance.data.w & GLIMMER_INSTANCE_FLAG_FOLIAGE) != 0u)
    {
        return;
    }

    // mostly cut out alpha tested cards (chains, grilles) aren't surfaces
    if ((instance.data.w & GLIMMER_INSTANCE_FLAG_ALPHA_TESTED) != 0u && GlimmerGetMaterialAverageAlbedoAlpha(instance.data.z).a < 0.5)
    {
        return;
    }

    for (uint chunkTriangle = groupIndex; chunkTriangle < GLIMMER_SPAN_CHUNK_TRIANGLES; chunkTriangle += GROUP_SIZE)
    {
        const uint localTriangle = asuint(chunk.boundsMax.w) + chunkTriangle;

        if (localTriangle >= instance.data.y)
        {
            break;
        }

        BinTriangle(instance, spanInstanceIndex, localTriangle, slotMask);
    }
#elif defined(MODE_ALLOC)
    if (threadIndex >= constants.options.z * BOX_BRICKS)
    {
        return;
    }

    const uint slot = threadIndex / BOX_BRICKS;
    const uint slotBrick = threadIndex % BOX_BRICKS;
    const int3 cell = SlotCell(slot, slotBrick);

    const uint tableIndex = GlimmerBrickTableIndex(cell, slotBrick % 3u);

    const uint coveredArea = glimmerBrickTable[tableIndex] + 1u;

    glimmerBrickTable[tableIndex] = GLIMMER_BRICK_NONE;

    // twigs, wires and the like aren't worth a brick
    if (glimmerBrickCounters[LIST_HEADS_BASE + threadIndex] == GLIMMER_BRICK_NONE || coveredArea < uint(MIN_BRICK_COLUMNS * AREA_SCALE))
    {
        return;
    }

    uint brick;

    if (!PopFree(glimmerBrickFreeList, COUNTER_FREE_BRICKS, slot, SLOT_FAILED_BRICKS, SLOT_BRICKS, brick))
    {
        return;
    }

    glimmerBrickTable[tableIndex] = brick;

    const uint header = GlimmerBrickWord(brick, 0u);

    glimmerBrickIndex[header] = 0u;

    for (uint cardSlot = 0; cardSlot < uint(GLIMMER_BRICK_CARDS); cardSlot++)
    {
        glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CARDS) + cardSlot] = GLIMMER_BRICK_NONE;
    }

    glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CARD_MASK)] = 0u;
    glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL)] = uint(cell.x);
    glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL) + 1u] = uint(cell.y);
    glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL) + 2u] = uint(cell.z);
    glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_AXIS)] = slotBrick % 3u;
    glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_SERIAL)] = (glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_SERIAL)] + 1u) & GLIMMER_BRICK_SERIAL_MASK;

    uint buildIndex;
    InterlockedAdd(glimmerBrickCounters[COUNTER_BUILD_BRICKS], 1u, buildIndex);

    glimmerBrickCounters[BUILD_LIST_BASE + 2u * buildIndex] = brick;
    glimmerBrickCounters[BUILD_LIST_BASE + 2u * buildIndex + 1u] = threadIndex;
#elif defined(MODE_ARGS)
    if (threadIndex != 0u)
    {
        return;
    }

    const uint numBuilt = glimmerBrickCounters[COUNTER_BUILD_BRICKS];

    WriteArgs(ARGS_COLUMNS, COLUMNS_PER_BRICK / GROUP_SIZE, numBuilt);
    WriteArgs(ARGS_TILES, (TILES_PER_BRICK + GROUP_SIZE - 1) / GROUP_SIZE, numBuilt);
    WriteArgs(ARGS_PREFIX, (numBuilt + GROUP_SIZE - 1u) / GROUP_SIZE, 1u);

    for (uint level = uint(GLIMMER_BRICK_CARD_FIRST_MIP); level < uint(GLIMMER_BRICK_MIPS); level++)
    {
        const uint side = uint(GLIMMER_BRICK_SIDE) >> level;

        WriteArgs(ARGS_MIPS + 3u * (level - uint(GLIMMER_BRICK_CARD_FIRST_MIP)), (uint(GLIMMER_BRICK_CARDS) * side * side + GROUP_SIZE - 1u) / GROUP_SIZE, numBuilt);
    }
#elif defined(MODE_PREFIX)
    if (threadIndex >= glimmerBrickCounters[COUNTER_BUILD_BRICKS])
    {
        return;
    }

    const uint brick = glimmerBrickCounters[BUILD_LIST_BASE + 2u * threadIndex];
    const uint slot = glimmerBrickCounters[BUILD_LIST_BASE + 2u * threadIndex + 1u] / BOX_BRICKS;

    int3 cell;
    uint axis;

    if (!GlimmerLoadBrickPlace(brick, cell, axis))
    {
        return;
    }

    const uint header = GlimmerBrickWord(brick, 0u);
    const uint columnsBase = header + uint(GLIMMER_BRICK_INDEX_COLUMNS);

    uint cursor = 0u;
    uint numSurfaceColumns = 0u;

    for (uint countColumn = 0; countColumn < COLUMNS_PER_BRICK; countColumn++)
    {
        const uint count = glimmerBrickIndex[columnsBase + countColumn] >> 24u;

        PlaceColumn(cursor, count);

        numSurfaceColumns += count != 0u ? 1u : 0u;
    }

    // what the bin took for enough surface was only its triangles' boxes
    if (numSurfaceColumns < uint(MIN_BRICK_COLUMNS))
    {
        Append(glimmerBrickFreeList, COUNTER_FREE_BRICKS, brick);

        glimmerBrickTable[GlimmerBrickTableIndex(cell, axis)] = GLIMMER_BRICK_NONE;
        glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_AXIS)] = GLIMMER_BRICK_NONE;

        InterlockedAdd(glimmerBrickCounters[SlotCounter(slot, SLOT_BRICKS)], 0xFFFFFFFFu);

        return;
    }

    uint numPages = (cursor + uint(GLIMMER_BRICK_PAGE_CROSSINGS) - 1u) / uint(GLIMMER_BRICK_PAGE_CROSSINGS);
    uint numFreePages = 0u;

    if (numPages != 0u && numPages <= uint(GLIMMER_BRICK_MAX_PAGES))
    {
        InterlockedAdd(glimmerBrickCounters[COUNTER_FREE_PAGES], 0u - numPages, numFreePages);

        if (int(numFreePages) < int(numPages))
        {
            InterlockedAdd(glimmerBrickCounters[COUNTER_FREE_PAGES], numPages);
            InterlockedAdd(glimmerBrickCounters[SlotCounter(slot, SLOT_FAILED_PAGES)], numPages);

            numFreePages = 0u;
        }
        else
        {
            InterlockedAdd(glimmerBrickCounters[SlotCounter(slot, SLOT_PAGES)], numPages);
        }
    }

    const bool hasPages = numFreePages != 0u;

    if (!hasPages && numPages != 0u)
    {
        InterlockedAdd(glimmerBrickCounters[COUNTER_DROPPED_COLUMNS], 1u);

        numPages = 0u;
    }

    glimmerBrickIndex[header] = numPages;

    for (uint pageIndex = 0; pageIndex < numPages; pageIndex++)
    {
        glimmerBrickIndex[header + 1u + pageIndex] = glimmerBrickPageFreeList[numFreePages - 1u - pageIndex];
    }

    cursor = 0u;

    for (uint placeColumn = 0; placeColumn < COLUMNS_PER_BRICK; placeColumn++)
    {
        const uint count = hasPages ? glimmerBrickIndex[columnsBase + placeColumn] >> 24u : 0u;
        const uint at = PlaceColumn(cursor, count);
        const uint first = count != 0u ? glimmerBrickIndex[header + 1u + at / uint(GLIMMER_BRICK_PAGE_CROSSINGS)] * uint(GLIMMER_BRICK_PAGE_CROSSINGS) + (at & (uint(GLIMMER_BRICK_PAGE_CROSSINGS) - 1u)) : 0u;

        glimmerBrickIndex[columnsBase + placeColumn] = first | (count << 24u);
    }
#elif defined(MODE_GATHER) || defined(MODE_COUNT) || defined(MODE_TILES)
    // a row of groups per brick of the build list
    const uint buildIndex = groupId.y;
    const uint withinBrick = groupId.x * GROUP_SIZE + groupIndex;

#if defined(MODE_GATHER) || defined(MODE_COUNT)
    const uint perBrick = COLUMNS_PER_BRICK;
#else
    const uint perBrick = TILES_PER_BRICK;
#endif

    if (buildIndex >= glimmerBrickCounters[COUNTER_BUILD_BRICKS] || withinBrick >= perBrick)
    {
        return;
    }

    const uint brick = glimmerBrickCounters[BUILD_LIST_BASE + 2u * buildIndex];
    const uint slotBrick = glimmerBrickCounters[BUILD_LIST_BASE + 2u * buildIndex + 1u];

    int3 cell;
    uint axis;

    // given back by the prefix pass
    if (!GlimmerLoadBrickPlace(brick, cell, axis))
    {
        return;
    }

#if defined(MODE_GATHER) || defined(MODE_COUNT)
    GatherColumn(brick, axis, cell, glimmerBrickCounters[LIST_HEADS_BASE + slotBrick], int2(withinBrick % GLIMMER_BRICK_SIDE, withinBrick / GLIMMER_BRICK_SIDE));
#else
    const uint slot = slotBrick / BOX_BRICKS;

    const int2 tile = int2(withinBrick % GLIMMER_BRICK_TILES_PER_SIDE, withinBrick / GLIMMER_BRICK_TILES_PER_SIDE);

    uint nearest = 0xFFFFu;
    uint furthest = 0u;

    for (uint tileColumn = 0; tileColumn < GLIMMER_BRICK_TILE * GLIMMER_BRICK_TILE; tileColumn++)
    {
        uint first;
        const uint count = GlimmerLoadBrickColumn(brick, tile * GLIMMER_BRICK_TILE + int2(tileColumn % GLIMMER_BRICK_TILE, tileColumn / GLIMMER_BRICK_TILE), first);

        for (uint k = 0; k < count; k++)
        {
            const uint code = glimmerBrickCrossings[2u * (first + k)] & 0xFFFFu;

            nearest = min(nearest, code);
            furthest = max(furthest, code);
        }
    }

    glimmerBrickIndex[GlimmerBrickTileIndex(brick, tile)] = nearest | (furthest << 16u);

    // the brick's first tile thread also takes a card for each one its crossings are on, when they are lit
    if (withinBrick == 0u && constants.options.w != 0u)
    {
        const uint header = GlimmerBrickWord(brick, 0u);
        const uint cardMask = glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CARD_MASK)];

        for (uint cardSlot = 0; cardSlot < uint(GLIMMER_BRICK_CARDS); cardSlot++)
        {
            uint card;

            if (((cardMask >> cardSlot) & 1u) != 0u && PopFree(glimmerBrickCardFreeList, COUNTER_FREE_CARDS, slot, SLOT_FAILED_CARDS, SLOT_CARDS, card))
            {
                glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CARDS) + cardSlot] = card;
            }
        }
    }
#endif
#elif defined(MODE_LIVE)
    if (constants.options.y == 0u)
    {
        if (threadIndex == 0u)
        {
            glimmerBrickCounters[COUNTER_LIVE_BRICKS] = 0u;
        }

        return;
    }

    if (threadIndex >= constants.counts.z || glimmerBrickIndex[GlimmerBrickWord(threadIndex, GLIMMER_BRICK_INDEX_AXIS)] > 2u)
    {
        return;
    }

    Append(glimmerBrickLiveList, COUNTER_LIVE_BRICKS, threadIndex);
#elif defined(MODE_GROW)
    const uint brick = uint(constants.boxMin.x) + threadIndex;
    const uint page = uint(constants.boxMin.y) + threadIndex;
    const uint card = uint(constants.boxMin.z) + threadIndex;

    if (brick < constants.counts.z)
    {
        glimmerBrickIndex[GlimmerBrickWord(brick, 0u)] = 0u;
        glimmerBrickIndex[GlimmerBrickWord(brick, GLIMMER_BRICK_INDEX_AXIS)] = GLIMMER_BRICK_NONE;

        Append(glimmerBrickFreeList, COUNTER_FREE_BRICKS, brick);
    }

    if (page < uint(constants.boxExtent.w))
    {
        Append(glimmerBrickPageFreeList, COUNTER_FREE_PAGES, page);
    }

    if (card < uint(constants.boxMin.w))
    {
        Append(glimmerBrickCardFreeList, COUNTER_FREE_CARDS, card);
    }
#endif
}
