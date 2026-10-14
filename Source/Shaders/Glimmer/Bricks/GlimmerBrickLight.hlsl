#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"

// LIGHT: the radiance each crossing of some bricks sends on diffusely, a thread per column
// MIPS: one mip of those bricks' cards, the first from the crossings and the rest from the one below
PERMUTE(MODE, LIGHT, MIPS)

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

#include "../GlimmerApply.hlsli"
#include "GlimmerBricks.hlsli"

struct GlimmerBrickLightConstants
{
    GlimmerBrickParams bricks;
    uint4 range;   // x = first place in the live list, y = bricks, z = 1 when they are the build list's, w = mip level
    float4 params; // x = columns off its surface a shadow ray starts, y = how far a shadow ray looks
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

#if defined(MODE_LIGHT)

float3 SunIrradiance(out float3 outToSun)
{
    const float3 toSun = world_shader_data.sun_direction_intensity.xyz;

    outToSun = dot(toSun, toSun) > 1e-8 ? normalize(toSun) : float3(0.0, 1.0, 0.0);

    const float aboveHorizon = saturate((outToSun.y + 0.1) / 0.15);

    return world_shader_data.sun_color.rgb * world_shader_data.sun_atmosphere_tint.rgb * (world_shader_data.sun_direction_intensity.w * aboveHorizon);
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
                sun = sunIrradiance * (NdotL / HYP_FMATH_PI);
            }
        }

        // rgb = irradiance / pi from the sky and what bounces
        const float4 indirect = EvaluateGlimmer(glimmer, P, N);

        const float3 emissive = GlimmerGetMaterialEmissive(packedMaterial & 0xFFFFu);
        const float3 radiance = crossing.color * (sun + max(indirect.rgb, 0.0)) + emissive;

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

        [unroll]
        for (uint child = 0; child < 4u; child++)
        {
            const uint2 under = texel * 2u + uint2(child & 1u, child >> 1u);

            mean += GlimmerBrickUnpackLight(glimmerBrickRadiance[from + under.y * side * 2u + under.x]) * 0.25;
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
}
