#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"

#include "../GlimmerCommon.hlsli"
#include "../GlimmerLight.hlsli"

DECLARE_SRV(GlimmerBrickGITrace, GlimmerBrickTableBuffer) StructuredBuffer<uint> glimmerBrickTable;
DECLARE_SRV(GlimmerBrickGITrace, GlimmerBrickIndexBuffer) StructuredBuffer<uint> glimmerBrickIndex;
DECLARE_SRV(GlimmerBrickGITrace, GlimmerBrickCrossingsBuffer) StructuredBuffer<uint> glimmerBrickCrossings;
DECLARE_SRV(GlimmerBrickGITrace, GlimmerBrickMaterialBuffer) StructuredBuffer<uint> glimmerBrickMaterial;
DECLARE_SRV(GlimmerBrickGITrace, GlimmerBrickCountersBuffer) StructuredBuffer<uint> glimmerBrickCounters;
DECLARE_SRV(GlimmerBrickGITrace, GlimmerBrickLiveListBuffer) StructuredBuffer<uint> glimmerBrickLiveList;
DECLARE_UAV(GlimmerBrickGITrace, GlimmerBrickSlotsBuffer) RWStructuredBuffer<uint> glimmerBrickSlots;

DECLARE_SRV(GlimmerBrickGITrace, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerBrickGITrace, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;
DECLARE_SRV(GlimmerBrickGITrace, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(GlimmerBrickGITrace, GlimmerHeightBoundsBuffer) StructuredBuffer<float> glimmerHeightBounds;

#include "GlimmerBricks.hlsli"
#include "GlimmerBrickGI.hlsli"

struct GlimmerBrickGITraceConstants
{
    GlimmerBrickParams bricks;
    uint4 range;   // x = first place in the live list, y = bricks, z = 1 when they are the build list's, w = 1 when the ground and spans can be traced
    float4 params; // x = columns off its surface a ray starts, y = how far a ray looks, z = foliage extinction, w = albedo where the ground's isn't known
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
};

DECLARE_BUFFER_DYNAMIC(GlimmerBrickGITrace, CBuffer) cbuffer CBuffer
{
    GlimmerBrickGITraceConstants constants;
};

// canopy is only counted as transmittance here: what it sends on is left to the cards it shades
GlimmerLight GlimmerCanopyRadiance(float3 P, float3 weightedAlbedo)
{
    return GlimmerNoLight();
}

#include "../GlimmerHeightfield.hlsli"

#define GROUP_SIZE 64
#define RAYS_PER_THREAD (GLIMMER_BRICK_GI_RAYS / 2)

uint BrickOfSlot(uint slot)
{
    if (constants.range.z == 0u)
    {
        const uint numLive = glimmerBrickCounters[COUNTER_LIVE_BRICKS];

        return slot < constants.range.y && slot < numLive ? glimmerBrickLiveList[(constants.range.x + slot) % numLive] : GLIMMER_BRICK_NONE;
    }

    if (slot >= glimmerBrickCounters[COUNTER_BUILD_BRICKS])
    {
        return GLIMMER_BRICK_NONE;
    }

    return glimmerBrickCounters[BUILD_LIST_BASE + 2u * slot];
}

// of the crossings on the card in a column, the one it shows: the nearest, or the furthest for a card facing along the axis
uint ShownCrossing(uint brick, uint axis, uint cardSlot, int2 column)
{
    const bool facesAlong = cardSlot >= uint(GLIMMER_BRICK_LAYERS);

    uint first;
    const uint count = GlimmerLoadBrickColumn(brick, column, first);

    uint shown = GLIMMER_BRICK_NONE;

    for (uint k = 0; k < count; k++)
    {
        const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(constants.bricks, first + k);

        if (GlimmerBrickCardSlot(constants.bricks, axis, crossing.normal, crossing.depth) == cardSlot && (shown == GLIMMER_BRICK_NONE || facesAlong))
        {
            shown = first + k;
        }
    }

    return shown;
}

uint TraceSlot(uint brick, int3 cell, uint axis, uint cardSlot, uint2 tile, uint ray, uint seed)
{
    // each ray from a column of its own where the card has one, the first free one after it otherwise
    const uint tileColumns = uint(GLIMMER_BRICK_TILE * GLIMMER_BRICK_TILE);
    const uint start = (ray * (tileColumns / uint(GLIMMER_BRICK_GI_RAYS)) + (seed & 1u)) % tileColumns;

    uint crossingIndex = GLIMMER_BRICK_NONE;
    int2 column = (int2)0;

    for (uint offset = 0; offset < tileColumns && crossingIndex == GLIMMER_BRICK_NONE; offset++)
    {
        const uint tileColumn = (start + offset) % tileColumns;

        column = int2(tile) * GLIMMER_BRICK_TILE + int2(tileColumn % uint(GLIMMER_BRICK_TILE), tileColumn / uint(GLIMMER_BRICK_TILE));
        crossingIndex = ShownCrossing(brick, axis, cardSlot, column);
    }

    if (crossingIndex == GLIMMER_BRICK_NONE)
    {
        return GLIMMER_BRICK_SLOT_NONE;
    }

    const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(constants.bricks, crossingIndex);

    const float3 P = GlimmerBrickCrossingPoint(constants.bricks, cell, axis, column, crossing);
    const float3 N = crossing.normal;

    const float3 direction = GlimmerBrickGIDirection(N, ray, seed);
    const float3 origin = P + N * (constants.params.x * constants.bricks.params.z);

    const float maxDistance = constants.params.y;
    const float bricksExit = GlimmerBrickWindowExit(constants.bricks, origin, direction);

    GlimmerBrickHit hit;

    if (TraceGlimmerBricks(constants.bricks, origin, direction, 0.0, bricksExit > 0.0 ? min(bricksExit, maxDistance) : maxDistance, hit))
    {
        if (dot(hit.normal, direction) > 0.0 && !GlimmerBrickHasFlag(glimmerBrickMaterial[hit.crossing], GLIMMER_BRICK_FLAG_DOUBLE_SIDED))
        {
            return GLIMMER_BRICK_SLOT_DARK;
        }

        return GlimmerBrickEncodeCrossingSlot(hit.crossing, glimmerBrickCrossings[2u * hit.crossing]);
    }

    float transmittance = 1.0;

    if (constants.range.w != 0u)
    {
        GlimmerHeightfieldHit heightfieldHit;

        if (GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, maxDistance, 0u, bricksExit, constants.params.z, false, heightfieldHit))
        {
            const float3 hitPosition = origin + direction * heightfieldHit.t;

            const float3 albedo = heightfieldHit.kind == GLIMMER_HEIGHTFIELD_GROUND
                ? GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, hitPosition.xz, heightfieldHit.level, (float3)constants.params.w)
                : heightfieldHit.albedo;

            return GlimmerBrickEncodeOutsideSlot(albedo * heightfieldHit.transmittance, heightfieldHit.normal);
        }

        transmittance = heightfieldHit.transmittance;
    }

    return GlimmerBrickEncodeSkySlot(direction, transmittance);
}

// a row of groups per brick, two threads per tile of each card
[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint withinBrick = groupId.x * GROUP_SIZE + groupIndex;
    const uint brick = BrickOfSlot(groupId.y);

    if (brick == GLIMMER_BRICK_NONE || withinBrick >= COLUMNS_PER_BRICK)
    {
        return;
    }

    const uint cardSlot = withinBrick / (2u * uint(GLIMMER_BRICK_GI_TILES));
    const uint tile = (withinBrick / 2u) % uint(GLIMMER_BRICK_GI_TILES);
    const uint pairIndex = withinBrick & 1u;

    int3 cell;
    uint axis;

    if (!GlimmerLoadBrickPlace(brick, cell, axis))
    {
        return;
    }

    const uint card = glimmerBrickIndex[GlimmerBrickWord(brick, uint(GLIMMER_BRICK_INDEX_CARDS) + cardSlot)];

    if (card == GLIMMER_BRICK_NONE)
    {
        return;
    }

    const uint2 tileCoord = uint2(tile % uint(GLIMMER_BRICK_CARD_SIDE), tile / uint(GLIMMER_BRICK_CARD_SIDE));
    const uint seed = GlimmerBrickGIHash(card * uint(GLIMMER_BRICK_GI_TILES) + tile);

    [loop]
    for (uint k = 0; k < uint(RAYS_PER_THREAD); k++)
    {
        const uint ray = pairIndex * uint(RAYS_PER_THREAD) + k;

        glimmerBrickSlots[GlimmerBrickSlotIndex(card, tile, ray)] = TraceSlot(brick, cell, axis, cardSlot, tileCoord, ray, seed);
    }
}
