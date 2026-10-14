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
DECLARE_SRV(DeferredPass, GlimmerBrickBuildListBuffer) StructuredBuffer<uint> glimmerBrickBuildList;
DECLARE_UAV(DeferredPass, GlimmerBrickRadianceBuffer) RWStructuredBuffer<uint> glimmerBrickRadiance;
DECLARE_UAV(DeferredPass, GlimmerBrickCrossingLightBuffer) RWStructuredBuffer<uint> glimmerBrickCrossingLight;

#include "../GlimmerApply.hlsli"
#include "GlimmerBricks.hlsli"

struct GlimmerBrickLightConstants
{
    GlimmerBrickParams bricks;
    int4 boxMin;    // xyz = absolute cell of the box the build list is of
    int4 boxExtent; // xyz = cells
    uint4 range;    // x = first brick, y = bricks, z = 1 when they are the build list's, w = mip level
    float4 params;  // x = columns off its surface a shadow ray starts, y = how far a shadow ray looks
    uint4 dispatch; // x = groups along x
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

// NOTE must match GlimmerBrickBuild.hlsl
#define COUNTER_BUILD_BRICKS 2

#define COLUMNS_PER_BRICK (GLIMMER_BRICK_SIDE * GLIMMER_BRICK_SIDE)

uint BrickOfSlot(uint slot)
{
    if (slot >= constants.range.y)
    {
        return GLIMMER_BRICK_NONE;
    }

    if (constants.range.z == 0u)
    {
        return constants.range.x + slot;
    }

    if (slot >= glimmerBrickCounters[COUNTER_BUILD_BRICKS])
    {
        return GLIMMER_BRICK_NONE;
    }

    const uint boxThread = glimmerBrickBuildList[slot];
    const uint cellIndex = boxThread / 3u;
    const uint3 extent = uint3(constants.boxExtent.xyz);

    const int3 cell = constants.boxMin.xyz + int3(cellIndex % extent.x, (cellIndex / extent.x) % extent.y, cellIndex / (extent.x * extent.y));

    return glimmerBrickTable[GlimmerBrickTableIndex(cell, boxThread % 3u)];
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
    const uint header = brick * uint(GLIMMER_BRICK_INDEX_STRIDE);
    const uint axis = glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_AXIS)];

    if (axis > 2u)
    {
        return;
    }

    const int3 cell = int3(
        int(glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL)]),
        int(glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL) + 1u]),
        int(glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CELL) + 2u]));

    const float width = constants.bricks.params.z;
    const float3 columnLow = GlimmerBrickCellLow(constants.bricks, cell) + GlimmerBrickPoint(axis, float2(column) * width, 0.0);

    const uint entry = glimmerBrickIndex[GlimmerBrickColumnIndex(brick, column)];
    const uint first = entry & 0xFFFFFFu;
    const uint count = min(entry >> 24u, uint(GLIMMER_BRICK_MAX_COLUMN_CROSSINGS));

    float3 toSun;
    const float3 sunIrradiance = SunIrradiance(toSun);

    for (uint k = 0; k < count; k++)
    {
        const uint crossingIndex = first + k;
        const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(constants.bricks, crossingIndex);
        const uint packedMaterial = glimmerBrickMaterial[crossingIndex];

        const float2 offset = (GlimmerBrickMaskMiddle(crossing.mask) - 0.5) * width;
        const float2 slope = GlimmerBrickAcross(axis, crossing.normal) / GlimmerBrickAlong(axis, crossing.normal);

        const float3 P = columnLow + GlimmerBrickPoint(axis, (float2)(0.5 * width) + offset, crossing.depth - dot(slope, offset));
        const float3 N = crossing.normal;

        const bool isDoubleSided = ((packedMaterial >> 29u) & GLIMMER_BRICK_FLAG_DOUBLE_SIDED) != 0u;

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
        const int2 column = int2(tile) * GLIMMER_BRICK_TILE + int2(tileColumn % GLIMMER_BRICK_TILE, tileColumn / GLIMMER_BRICK_TILE);

        const uint entry = glimmerBrickIndex[GlimmerBrickColumnIndex(brick, column)];
        const uint first = entry & 0xFFFFFFu;
        const uint count = min(entry >> 24u, uint(GLIMMER_BRICK_MAX_COLUMN_CROSSINGS));

        uint shown = GLIMMER_BRICK_NONE;

        // crossings are nearest first
        for (uint k = 0; k < count; k++)
        {
            const uint words = glimmerBrickCrossings[2u * (first + k)];
            const uint slot = GlimmerBrickCardSlot(constants.bricks, axis, GlimmerBrickUnpackNormal(words >> 16u), GlimmerBrickUnpackDepth(constants.bricks, words & 0xFFFFu));

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
    const uint threadIndex = (groupId.y * constants.dispatch.x + groupId.x) * GROUP_SIZE + groupIndex;

#if defined(MODE_LIGHT)
    const uint brick = BrickOfSlot(threadIndex / COLUMNS_PER_BRICK);

    if (brick == GLIMMER_BRICK_NONE)
    {
        return;
    }

    const uint columnIndex = threadIndex % COLUMNS_PER_BRICK;

    LightColumn(brick, int2(columnIndex % GLIMMER_BRICK_SIDE, columnIndex / GLIMMER_BRICK_SIDE));
#else
    const uint level = constants.range.w;
    const uint side = uint(GLIMMER_BRICK_SIDE) >> level;
    const uint texelsPerCard = side * side;

    const uint cardOrdinal = threadIndex / texelsPerCard;
    const uint texelIndex = threadIndex % texelsPerCard;

    const uint brick = BrickOfSlot(cardOrdinal / uint(GLIMMER_BRICK_CARDS));

    if (brick == GLIMMER_BRICK_NONE)
    {
        return;
    }

    const uint header = brick * uint(GLIMMER_BRICK_INDEX_STRIDE);
    const uint axis = glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_AXIS)];

    if (axis > 2u)
    {
        return;
    }

    const uint cardSlot = cardOrdinal % uint(GLIMMER_BRICK_CARDS);
    const uint card = glimmerBrickIndex[header + uint(GLIMMER_BRICK_INDEX_CARDS) + cardSlot];

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
        glimmerBrickRadiance[outIndex] = GlimmerBrickPackLight(mean.rgb, clamp(uint(mean.a * 7.0 + 0.5), 1u, 7u));
    }
#endif
}
