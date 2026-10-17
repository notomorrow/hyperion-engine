#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"

// LIGHT: the radiance each crossing of some bricks sends on diffusely, a thread per column
// MIPS: one mip of those bricks' cards, the first from the crossings and the rest from the one below
// GATHER: what arrives at each tile of those bricks' cards from what its slots found, a thread per tile
PERMUTE(MODE, LIGHT, MIPS, GATHER)

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#include "../../Include/EnvProbes.hlsli"

DECLARE_SRV(DeferredPass, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV(DeferredPass, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(DeferredPass, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(DeferredPass, GlimmerBrickTableBuffer) StructuredBuffer<uint> glimmerBrickTable;
DECLARE_SRV(DeferredPass, GlimmerBrickIndexBuffer) StructuredBuffer<uint> glimmerBrickIndex;
DECLARE_SRV(DeferredPass, GlimmerBrickCrossingsBuffer) StructuredBuffer<uint> glimmerBrickCrossings;
DECLARE_SRV(DeferredPass, GlimmerBrickMaterialBuffer) StructuredBuffer<uint> glimmerBrickMaterial;
DECLARE_SRV(DeferredPass, GlimmerBrickCountersBuffer) StructuredBuffer<uint> glimmerBrickCounters;
DECLARE_SRV(DeferredPass, GlimmerBrickLiveListBuffer) StructuredBuffer<uint> glimmerBrickLiveList;
DECLARE_UAV(DeferredPass, GlimmerBrickRadianceBuffer) RWStructuredBuffer<uint> glimmerBrickRadiance;
DECLARE_UAV(DeferredPass, GlimmerBrickCrossingLightBuffer) RWStructuredBuffer<uint> glimmerBrickCrossingLight;
DECLARE_SRV(DeferredPass, GlimmerBrickSlotsBuffer) StructuredBuffer<uint> glimmerBrickSlots;
DECLARE_UAV(DeferredPass, GlimmerBrickArrivingBuffer) RWStructuredBuffer<uint> glimmerBrickArriving;

DECLARE_SRV(DeferredPass, GlimmerGroundGIBuffer) StructuredBuffer<uint> glimmerGroundGI;
DECLARE_SRV(DeferredPass, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(DeferredPass, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(DeferredPass, GlimmerHeightBoundsBuffer) StructuredBuffer<float> glimmerHeightBounds;

#include "../GlimmerApply.hlsli"
#include "../GlimmerLight.hlsli"
#include "GlimmerBricks.hlsli"
#include "GlimmerBrickGI.hlsli"
#include "../GlimmerGroundGI.hlsli"

struct GlimmerBrickLightConstants
{
    GlimmerBrickParams bricks;
    uint4 range;   // x = first place in the live list, y = bricks, z = 1 when they are the build list's, w = mip level
    float4 params; // x = columns off its surface a shadow ray starts, y = how far a shadow ray looks, z = 1 when the indirect light is the slots' (brick GI), w = foliage extinction
    uint4 info;    // x = 1 when the ground cache can be read, y = 1 when a shadow ray goes on through the heightfield
    GlimmerLightParams light;
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    GlimmerRelightParams lit; // the ground cache's lit windows
};

DECLARE_BUFFER_DYNAMIC(DeferredPass, CBuffer) cbuffer CBuffer
{
    GlimmerApply glimmer;
    GlimmerBrickLightConstants constants;
};

#define GLIMMER_APPLY_WITH_SAMPLING
#include "../GlimmerApply.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../GlimmerMaterial.hlsli"

// canopy is only counted as transmittance in a shadow ray
GlimmerLight GlimmerCanopyRadiance(float3 P, float3 weightedAlbedo)
{
    return GlimmerNoLight();
}

#include "../GlimmerGround.hlsli"
#include "../GlimmerHeightfield.hlsli"

#define GROUP_SIZE 64

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

float3 SunIrradiance(out float3 outToSun)
{
    const float3 toSun = world_shader_data.sun_direction_intensity.xyz;

    outToSun = dot(toSun, toSun) > 1e-8 ? normalize(toSun) : float3(0.0, 1.0, 0.0);

    const float aboveHorizon = saturate((outToSun.y + 0.1) / 0.15);

    return world_shader_data.sun_color.rgb * world_shader_data.sun_atmosphere_tint.rgb * (world_shader_data.sun_direction_intensity.w * aboveHorizon);
}

bool IsBrickGI()
{
    return constants.params.z != 0.0;
}

#if defined(MODE_LIGHT)

// irradiance / pi the slots of the crossing's tile gathered, without the sun on the crossing itself
float3 ArrivingAt(uint brick, uint axis, int2 column, GlimmerBrickCrossing crossing)
{
    const uint cardSlot = GlimmerBrickCardSlot(constants.bricks, axis, crossing.normal, crossing.depth);
    const uint card = glimmerBrickIndex[GlimmerBrickWord(brick, uint(GLIMMER_BRICK_INDEX_CARDS) + cardSlot)];

    if (card == GLIMMER_BRICK_NONE)
    {
        return (float3)0.0;
    }

    // a tile's few rays are noisy alone: a tent two tiles wide each way around the column, over the same card's tiles, weighted by how much of each is surface
    const float2 at = (float2(column) + 0.5) / float(GLIMMER_BRICK_TILE) - 0.5;
    const int2 first = int2(floor(at)) - 1;

    float4 sum = (float4)0.0;

    [unroll]
    for (int dy = 0; dy < 4; dy++)
    {
        [unroll]
        for (int dx = 0; dx < 4; dx++)
        {
            const int2 tile = first + int2(dx, dy);

            if (any(tile < 0) || any(tile >= GLIMMER_BRICK_CARD_SIDE))
            {
                continue;
            }

            const float2 apart = abs(at - float2(tile));
            const float weight = max(2.0 - apart.x, 0.0) * max(2.0 - apart.y, 0.0);

            sum += GlimmerBrickUnpackLight(glimmerBrickArriving[card * uint(GLIMMER_BRICK_CARD_TEXELS) + uint(tile.y) * uint(GLIMMER_BRICK_CARD_SIDE) + uint(tile.x)]) * weight;
        }
    }

    return sum.a > 0.0 ? sum.rgb / sum.a : (float3)0.0;
}

void LightColumn(uint brick, int2 column)
{
    int3 cell;
    uint axis;

    if (!GlimmerLoadBrickPlace(brick, cell, axis))
    {
        return;
    }

    const float width = constants.bricks.params.z;

    uint first;
    const uint count = GlimmerLoadBrickColumn(brick, column, first);

    float3 toSun;
    const float3 sunIrradiance = SunIrradiance(toSun);

    for (uint k = 0; k < count; k++)
    {
        const uint crossingIndex = first + k;
        const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(constants.bricks, crossingIndex);
        const uint packedMaterial = glimmerBrickMaterial[crossingIndex];

        const float3 P = GlimmerBrickCrossingPoint(constants.bricks, cell, axis, column, crossing);
        const float3 N = crossing.normal;

        const bool isDoubleSided = GlimmerBrickHasFlag(packedMaterial, GLIMMER_BRICK_FLAG_DOUBLE_SIDED);

        float NdotL = dot(N, toSun);
        float3 shadowOrigin = P + N * (constants.params.x * width);

        if (isDoubleSided && NdotL < 0.0)
        {
            NdotL = -NdotL;
            shadowOrigin = P - N * (constants.params.x * width);
        }

        float3 sun = (float3)0.0;

        if (NdotL > 0.0 && any(sunIrradiance > 0.0))
        {
            GlimmerBrickHit shadowHit;

            if (!TraceGlimmerBricks(constants.bricks, shadowOrigin, toSun, 0.0, constants.params.y, shadowHit))
            {
                float visibility = 1.0;

                // past the bricks: the terrain, the spans' solids and the canopy
                if (constants.info.y != 0u)
                {
                    GlimmerHeightfieldHit heightfieldHit;

                    visibility = GlimmerTraceHeightfield(constants.ground, constants.spans, shadowOrigin, toSun, constants.params.y, 0u, GlimmerBrickWindowExit(constants.bricks, shadowOrigin, toSun), constants.params.w, false, heightfieldHit)
                        ? 0.0
                        : heightfieldHit.transmittance;
                }

                sun = sunIrradiance * (visibility * NdotL / HYP_FMATH_PI);
            }
        }

        // irradiance / pi from the sky and what bounces
        const float3 indirect = IsBrickGI()
            ? ArrivingAt(brick, axis, column, crossing) * glimmer.settings.x
            : max(EvaluateGlimmer(glimmer, P, N).rgb, 0.0);

        const float3 emissive = GlimmerGetMaterialEmissive(packedMaterial & 0xFFFFu);
        const float3 radiance = crossing.color * (sun + indirect) + emissive;

        glimmerBrickCrossingLight[crossingIndex] = GlimmerBrickPackLight(radiance, 7u);
    }
}

#else

// of the crossings on the card in a column, it shows the one furthest the way it faces
float4 TileLight(uint brick, uint axis, uint cardSlot, uint2 tile)
{
    const bool facesAlong = cardSlot >= uint(GLIMMER_BRICK_LAYERS);

    float4 sum = (float4)0.0;

    for (uint tileColumn = 0; tileColumn < uint(GLIMMER_BRICK_TILE * GLIMMER_BRICK_TILE); tileColumn++)
    {
        uint first;
        const uint count = GlimmerLoadBrickColumn(brick, int2(tile) * GLIMMER_BRICK_TILE + int2(tileColumn % GLIMMER_BRICK_TILE, tileColumn / GLIMMER_BRICK_TILE), first);

        uint shown = GLIMMER_BRICK_NONE;

        // crossings are nearest first
        for (uint k = 0; k < count; k++)
        {
            const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(constants.bricks, first + k);
            const uint slot = GlimmerBrickCardSlot(constants.bricks, axis, crossing.normal, crossing.depth);

            if (slot == cardSlot && (shown == GLIMMER_BRICK_NONE || facesAlong))
            {
                shown = first + k;
            }
        }

        if (shown != GLIMMER_BRICK_NONE)
        {
            sum += float4(GlimmerBrickUnpackLight(glimmerBrickCrossingLight[shown]).rgb, 1.0);
        }
    }

    return sum / float(GLIMMER_BRICK_TILE * GLIMMER_BRICK_TILE);
}

// radiance coming back along a slot's ray, false for a slot with nothing to say: not traced, or its crossing's place since taken by another
// near is the xz of the slot's brick, to place a ground slot's hit
bool SlotRadiance(uint slot, float2 near, float3 toSun, float3 sunIrradiance, out float3 outRadiance)
{
    outRadiance = (float3)0.0;

    const uint kind = GlimmerBrickSlotKind(slot);

    if (kind == GLIMMER_BRICK_SLOT_NONE)
    {
        return false;
    }

    if (kind == GLIMMER_BRICK_SLOT_CROSSING)
    {
        const uint crossing = slot & 0xFFFFFFu;

        if (((slot >> 24u) & 63u) != GlimmerBrickCrossingHash(glimmerBrickCrossings[2u * crossing]))
        {
            return false;
        }

        outRadiance = GlimmerBrickUnpackLight(glimmerBrickCrossingLight[crossing]).rgb;

        return true;
    }

    if (kind == GLIMMER_BRICK_SLOT_SKY)
    {
        const float transmittance = float((slot >> 20u) & 0xFFu) / 255.0;

        outRadiance = GlimmerBrickSkyRadiance(constants.light, GlimmerBrickUnpackOctahedral(slot & 0xFFFFFu, 10u)) * transmittance;

        return true;
    }

    if (slot == GLIMMER_BRICK_SLOT_DARK)
    {
        return true;
    }

    if (GlimmerBrickIsGroundSlot(slot))
    {
        if (constants.info.x == 0u)
        {
            return false;
        }

        float transmittance;
        const float2 xz = GlimmerBrickDecodeGroundSlot(slot, near, transmittance);

        float3 sent;

        if (!GlimmerSampleGroundGI(constants.lit, constants.ground, xz, GLIMMER_GROUND_GI_SENT, sent))
        {
            return false;
        }

        outRadiance = sent * transmittance;

        return true;
    }

    // past the bricks: lit by the sun, with nothing in its way counted, and the open sky
    const float3 albedo = GlimmerBrickUnpackColor(slot & 0xFFFFu);
    const float3 N = GlimmerBrickUnpackOctahedral((slot >> 16u) & 0xFFFu, 6u);

    outRadiance = albedo * (sunIrradiance * (max(dot(N, toSun), 0.0) / HYP_FMATH_PI) + GlimmerBrickSkyIrradiance(constants.light, N));

    return true;
}

void GatherTile(uint card, uint tile, float2 near)
{
    float3 toSun;
    const float3 sunIrradiance = SunIrradiance(toSun);

    float3 sum = (float3)0.0;
    uint numTraced = 0u;
    uint numKnown = 0u;

    for (uint ray = 0; ray < uint(GLIMMER_BRICK_GI_RAYS); ray++)
    {
        const uint slot = glimmerBrickSlots[GlimmerBrickSlotIndex(card, tile, ray)];

        numTraced += slot != GLIMMER_BRICK_SLOT_NONE ? 1u : 0u;

        float3 radiance;

        if (SlotRadiance(slot, near, toSun, sunIrradiance, radiance))
        {
            sum += radiance;
            numKnown++;
        }
    }

    const uint outIndex = card * uint(GLIMMER_BRICK_CARD_TEXELS) + tile;

    // a tile none of whose crossings' places are still theirs keeps what it had until it is traced again
    if (numTraced == 0u)
    {
        glimmerBrickArriving[outIndex] = 0u;
    }
    else if (numKnown != 0u)
    {
        glimmerBrickArriving[outIndex] = GlimmerBrickPackLight(sum / float(numKnown), 7u);
    }
}

#endif

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    // a row of groups per brick
    const uint withinBrick = groupId.x * GROUP_SIZE + groupIndex;

#if defined(MODE_LIGHT)
    const uint brick = BrickOfSlot(groupId.y);

    if (brick == GLIMMER_BRICK_NONE || withinBrick >= COLUMNS_PER_BRICK)
    {
        return;
    }

    LightColumn(brick, int2(withinBrick % GLIMMER_BRICK_SIDE, withinBrick / GLIMMER_BRICK_SIDE));
#else
    const uint level = constants.range.w;
    const uint side = uint(GLIMMER_BRICK_SIDE) >> level;
    const uint texelsPerCard = side * side;

    const uint cardSlot = withinBrick / texelsPerCard;
    const uint texelIndex = withinBrick % texelsPerCard;

    const uint brick = BrickOfSlot(groupId.y);

    if (brick == GLIMMER_BRICK_NONE || cardSlot >= uint(GLIMMER_BRICK_CARDS))
    {
        return;
    }

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

#if defined(MODE_GATHER)
    GatherTile(card, texelIndex, GlimmerBrickCellLow(constants.bricks, cell).xz + 0.5 * constants.bricks.params.x);
#else
    const uint base = card * uint(GLIMMER_BRICK_CARD_TEXELS);
    const uint2 texel = uint2(texelIndex % side, texelIndex / side);

    // rgb = the mean light with a texel that has no surface counting as dark, a = how much of it is surface
    float4 mean = (float4)0.0;

    if (level == uint(GLIMMER_BRICK_CARD_FIRST_MIP))
    {
        mean = TileLight(brick, axis, cardSlot, texel);
    }
    else
    {
        const uint from = base + GlimmerBrickCardMipOffset(level - 1u);

        float4 arriving = (float4)0.0;

        [unroll]
        for (uint child = 0; child < 4u; child++)
        {
            const uint2 under = texel * 2u + uint2(child & 1u, child >> 1u);

            mean += GlimmerBrickUnpackLight(glimmerBrickRadiance[from + under.y * side * 2u + under.x]) * 0.25;

            if (IsBrickGI())
            {
                arriving += GlimmerBrickUnpackLight(glimmerBrickArriving[from + under.y * side * 2u + under.x]) * 0.25;
            }
        }

        if (IsBrickGI())
        {
            const uint arrivingIndex = base + GlimmerBrickCardMipOffset(level) + texelIndex;
            const uint arrivingShare = clamp(uint(arriving.a * 7.0 + 0.5), 1u, 7u);

            glimmerBrickArriving[arrivingIndex] = arriving.a == 0.0 ? 0u : GlimmerBrickPackLight(arriving.rgb / arriving.a * (float(arrivingShare) / 7.0), arrivingShare);
        }
    }

    const uint outIndex = base + GlimmerBrickCardMipOffset(level) + texelIndex;

    if (mean.a == 0.0)
    {
        glimmerBrickRadiance[outIndex] = 0u;
    }
    else
    {
        // kept times the share as rounded, so that dividing it out gives the mean of the surface under the texel
        const uint share = clamp(uint(mean.a * 7.0 + 0.5), 1u, 7u);

        glimmerBrickRadiance[outIndex] = GlimmerBrickPackLight(mean.rgb / mean.a * (float(share) / 7.0), share);
    }
#endif
#endif
}
