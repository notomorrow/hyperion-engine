#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Material.hlsli"
#include "../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GlimmerCommon.hlsli"
#include "GlimmerLight.hlsli"

DECLARE_SRV(GlimmerGroundGI, WorldsBuffer) StructuredBuffer<WorldShaderData> _worlds_buffer;
#define world_shader_data _worlds_buffer[0]

DECLARE_SRV(GlimmerGroundGI, GlimmerBrickTableBuffer) StructuredBuffer<uint> glimmerBrickTable;
DECLARE_SRV(GlimmerGroundGI, GlimmerBrickIndexBuffer) StructuredBuffer<uint> glimmerBrickIndex;
DECLARE_SRV(GlimmerGroundGI, GlimmerBrickCrossingsBuffer) StructuredBuffer<uint> glimmerBrickCrossings;
DECLARE_SRV(GlimmerGroundGI, GlimmerBrickMaterialBuffer) StructuredBuffer<uint> glimmerBrickMaterial;
DECLARE_SRV(GlimmerGroundGI, GlimmerBrickCrossingLightBuffer) StructuredBuffer<uint> glimmerBrickCrossingLight;

DECLARE_SRV(GlimmerGroundGI, GlimmerGroundTexture) Texture2DArray<float> glimmerGround;
DECLARE_SRV(GlimmerGroundGI, GlimmerGroundAlbedoTexture) Texture2DArray<float4> glimmerGroundAlbedo;
DECLARE_SRV(GlimmerGroundGI, GlimmerSpansBuffer) StructuredBuffer<uint> glimmerSpans;
DECLARE_SRV(GlimmerGroundGI, GlimmerHeightBoundsBuffer) StructuredBuffer<float> glimmerHeightBounds;

DECLARE_UAV(GlimmerGroundGI, GlimmerGroundGIBuffer) RWStructuredBuffer<uint> glimmerGroundGI;

#include "Bricks/GlimmerBricks.hlsli"
#include "Bricks/GlimmerBrickGI.hlsli"
#include "GlimmerRelight.hlsli"

struct GlimmerGroundGIConstants
{
    GlimmerBrickParams bricks; // origin.w = 0 without bricks
    GlimmerGroundParams ground;
    GlimmerSpanParams spans;
    GlimmerRelightParams lit;  // the lit windows as the last update left them, for the ground the slots landed on
    GlimmerLightParams light;
    int4 rect;                 // xy = first texel of the dispatch, zw = past its last
    uint4 info;                // x = level, y = 1 when the spans can be traced
    float4 params;             // x = foliage extinction, y = how far a ray looks, z = albedo where the ground's isn't known, w = Rendering.Glimmer.Intensity
};

DECLARE_BUFFER_DYNAMIC(GlimmerGroundGI, CBuffer) cbuffer CBuffer
{
    GlimmerGroundGIConstants constants;
};

// what the canopy a ray passes through sends along it: defined below, once the cache can be read
GlimmerLight GlimmerCanopyRadiance(float3 P, float3 weightedAlbedo);

#include "GlimmerGround.hlsli"
#include "GlimmerHeightfield.hlsli"
#include "GlimmerGroundGI.hlsli"

#define GROUP_SIZE 64

// metres off the ground a ray starts
#define GLIMMER_GROUND_GI_BIAS 0.2

// metres off the canopy's top or bottom its rays start
#define GLIMMER_CANOPY_GI_BIAS 0.3

float3 SunIrradiance(out float3 outToSun)
{
    const float3 toSun = world_shader_data.sun_direction_intensity.xyz;

    outToSun = dot(toSun, toSun) > 1e-8 ? normalize(toSun) : float3(0.0, 1.0, 0.0);

    const float aboveHorizon = saturate((outToSun.y + 0.1) / 0.15);

    return world_shader_data.sun_color.rgb * world_shader_data.sun_atmosphere_tint.rgb * (world_shader_data.sun_direction_intensity.w * aboveHorizon);
}

// the leaves light from all round; the rest of what a ray carries is radiance in the rest part
GlimmerLight GlimmerCanopyRadiance(float3 P, float3 weightedAlbedo)
{
    float3 toSun;
    const float3 sunIrradiance = SunIrradiance(toSun);

    float3 irradiance;

    if (!GlimmerSampleCanopyGI(constants.lit, constants.ground, constants.spans, P, (float3)0.0, constants.params.x, toSun, sunIrradiance, irradiance))
    {
        return GlimmerNoLight();
    }

    return GlimmerRestLight(weightedAlbedo * irradiance);
}

// a ray through the bricks to where it leaves them, then the heightfield, with what the canopy on the way sends along it
uint TraceSlot(float3 origin, float3 direction, uint level, out float3 outInscatter)
{
    outInscatter = (float3)0.0;

    const float maxDistance = constants.params.y;
    const float bricksExit = GlimmerBrickWindowExit(constants.bricks, origin, direction);

    GlimmerBrickHit hit;

    if (bricksExit > 0.0 && TraceGlimmerBricks(constants.bricks, origin, direction, 0.0, min(bricksExit, maxDistance), hit))
    {
        if (dot(hit.normal, direction) > 0.0 && !GlimmerBrickHasFlag(glimmerBrickMaterial[hit.crossing], GLIMMER_BRICK_FLAG_DOUBLE_SIDED))
        {
            return GLIMMER_BRICK_SLOT_DARK;
        }

        return GlimmerBrickEncodeCrossingSlot(hit.crossing, glimmerBrickCrossings[2u * hit.crossing]);
    }

    float transmittance = 1.0;

    if (constants.info.y != 0u)
    {
        GlimmerHeightfieldHit heightfieldHit;

        const bool isHit = GlimmerTraceHeightfield(constants.ground, constants.spans, origin, direction, maxDistance, level, bricksExit, constants.params.x, true, heightfieldHit);

        outInscatter = heightfieldHit.inscatter.parts[GLIMMER_LIGHT_REST];

        if (isHit)
        {
            const float3 hitPosition = origin + direction * heightfieldHit.t;

            if (heightfieldHit.kind == GLIMMER_HEIGHTFIELD_GROUND)
            {
                return GlimmerBrickEncodeGroundSlot(hitPosition.xz, heightfieldHit.transmittance);
            }

            return GlimmerBrickEncodeOutsideSlot(heightfieldHit.albedo * heightfieldHit.transmittance, heightfieldHit.normal);
        }

        transmittance = heightfieldHit.transmittance;
    }

    return GlimmerBrickEncodeSkySlot(direction, transmittance);
}

// how much of the sun gets to a point on the ground: nothing in the bricks or the heightfield's solids, then through the canopy
float SunVisibility(float3 origin, float3 toSun, uint level)
{
    const float bricksExit = GlimmerBrickWindowExit(constants.bricks, origin, toSun);

    GlimmerBrickHit hit;

    if (bricksExit > 0.0 && TraceGlimmerBricks(constants.bricks, origin, toSun, 0.0, bricksExit, hit))
    {
        return 0.0;
    }

    if (constants.info.y == 0u)
    {
        return 1.0;
    }

    GlimmerHeightfieldHit heightfieldHit;

    if (GlimmerTraceHeightfield(constants.ground, constants.spans, origin, toSun, constants.params.y, level, bricksExit, constants.params.x, false, heightfieldHit))
    {
        return 0.0;
    }

    return heightfieldHit.transmittance;
}

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

    const float3 albedo = GlimmerBrickUnpackColor(slot & 0xFFFFu);
    const float3 N = GlimmerBrickUnpackOctahedral((slot >> 16u) & 0xFFFu, 6u);

    outRadiance = albedo * (sunIrradiance * (max(dot(N, toSun), 0.0) / HYP_FMATH_PI) + GlimmerBrickSkyIrradiance(constants.light, N));

    return true;
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const int2 extent = constants.rect.zw - constants.rect.xy;
    const uint index = dispatchThreadId.x;

    if (index >= uint(extent.x * extent.y))
    {
        return;
    }

    const int2 texel = constants.rect.xy + int2(int(index % uint(extent.x)), int(index / uint(extent.x)));
    const uint level = constants.info.x;
    const uint texelIndex = GlimmerGroundGITexelIndex(level, texel);

    const float2 xz = (float2(texel) + 0.5) * constants.ground.levels[level].params.x;

    float height;

    if (!GlimmerSampleGroundLevel(constants.ground, level, xz, height))
    {
        [unroll]
        for (uint clearRay = 0; clearRay < uint(GLIMMER_GROUND_GI_RAYS); clearRay++)
        {
            glimmerGroundGI[texelIndex * uint(GLIMMER_GROUND_GI_RAYS) + clearRay] = 0u;
        }

        glimmerGroundGI[GLIMMER_GROUND_GI_ARRIVING + texelIndex] = 0u;
        glimmerGroundGI[GLIMMER_GROUND_GI_SENT + texelIndex] = 0u;
        glimmerGroundGI[GLIMMER_GROUND_GI_CANOPY_TOP + texelIndex] = 0u;
        glimmerGroundGI[GLIMMER_GROUND_GI_CANOPY_BOTTOM + texelIndex] = 0u;
        glimmerGroundGI[GLIMMER_GROUND_GI_CANOPY_SUN + texelIndex] = 0u;

        return;
    }

    const float3 P = float3(xz.x, height, xz.y);
    const float3 N = GlimmerGroundNormal(constants.ground, xz, level);
    const float3 origin = P + N * GLIMMER_GROUND_GI_BIAS;

    const float3 albedo = GlimmerSampleGroundAlbedo(glimmerGroundAlbedo, constants.ground, xz, level, (float3)constants.params.z);

    float3 toSun;
    const float3 sunIrradiance = SunIrradiance(toSun);

    const uint seed = GlimmerBrickGIHash(texelIndex * 31u + 7u);

    float3 sum = (float3)0.0;
    uint numKnown = 0u;

    for (uint ray = 0; ray < uint(GLIMMER_GROUND_GI_RAYS); ray++)
    {
        float3 inscatter;
        const uint slot = TraceSlot(origin, GlimmerBrickGIDirection(N, ray, seed), level, inscatter);

        glimmerGroundGI[texelIndex * uint(GLIMMER_GROUND_GI_RAYS) + ray] = slot;

        float3 radiance;

        if (SlotRadiance(slot, P.xz, toSun, sunIrradiance, radiance))
        {
            sum += radiance + inscatter;
            numKnown++;
        }
    }

    const float3 arriving = numKnown != 0u ? sum / float(numKnown) : (float3)0.0;

    const float NdotL = dot(N, toSun);
    const float visibility = NdotL > 0.0 && any(sunIrradiance > 0.0) ? SunVisibility(origin, toSun, level) : 0.0;

    const float3 sent = albedo * (sunIrradiance * (visibility * max(NdotL, 0.0) / HYP_FMATH_PI) + arriving * constants.params.w);

    glimmerGroundGI[GLIMMER_GROUND_GI_ARRIVING + texelIndex] = GlimmerBrickPackLight(arriving, 7u);
    glimmerGroundGI[GLIMMER_GROUND_GI_SENT + texelIndex] = GlimmerBrickPackLight(sent, 7u);

    // the canopy over the texel: what arrives at its top from above, at its bottom from below, and the sun on its top
    uint canopyTop = 0u;
    uint canopyBottom = 0u;
    uint canopySun = 0u;

    GlimmerSpanSample spanSample;

    if (constants.info.y != 0u && GlimmerSampleSpans(constants.spans, level, xz, spanSample) && spanSample.canopyBins != 0u)
    {
        const float bottom = spanSample.canopyMin;
        const float top = bottom + GlimmerSpanHeight(spanSample.canopyMin, spanSample.canopyMax);

        const float3 topOrigin = float3(xz.x, top + GLIMMER_CANOPY_GI_BIAS, xz.y);
        const float3 bottomOrigin = float3(xz.x, max(bottom - GLIMMER_CANOPY_GI_BIAS, height + GLIMMER_GROUND_GI_BIAS), xz.y);

        float3 sumTop = (float3)0.0;
        float3 sumBottom = (float3)0.0;
        uint numTop = 0u;
        uint numBottom = 0u;

        for (uint canopyRay = 0; canopyRay < uint(GLIMMER_GROUND_GI_CANOPY_RAYS); canopyRay++)
        {
            // every other stratum of the full set, so the few rays still spread over the hemisphere
            const uint stratum = canopyRay * uint(GLIMMER_BRICK_GI_RAYS / GLIMMER_GROUND_GI_CANOPY_RAYS);

            float3 inscatter;
            float3 radiance;

            const uint slotUp = TraceSlot(topOrigin, GlimmerBrickGIDirection(float3(0.0, 1.0, 0.0), stratum, seed), level, inscatter);

            if (SlotRadiance(slotUp, xz, toSun, sunIrradiance, radiance))
            {
                sumTop += radiance + inscatter;
                numTop++;
            }

            const uint slotDown = TraceSlot(bottomOrigin, GlimmerBrickGIDirection(float3(0.0, -1.0, 0.0), stratum, seed + 1u), level, inscatter);

            if (SlotRadiance(slotDown, xz, toSun, sunIrradiance, radiance))
            {
                sumBottom += radiance + inscatter;
                numBottom++;
            }
        }

        canopyTop = GlimmerBrickPackLight(numTop != 0u ? sumTop / float(numTop) : (float3)0.0, 7u);
        canopyBottom = GlimmerBrickPackLight(numBottom != 0u ? sumBottom / float(numBottom) : (float3)0.0, 7u);

        const float canopySunVisibility = toSun.y > 0.0 && any(sunIrradiance > 0.0) ? SunVisibility(topOrigin, toSun, level) : 0.0;

        canopySun = 1u + uint(saturate(canopySunVisibility) * 65534.0 + 0.5);
    }

    glimmerGroundGI[GLIMMER_GROUND_GI_CANOPY_TOP + texelIndex] = canopyTop;
    glimmerGroundGI[GLIMMER_GROUND_GI_CANOPY_BOTTOM + texelIndex] = canopyBottom;
    glimmerGroundGI[GLIMMER_GROUND_GI_CANOPY_SUN + texelIndex] = canopySun;
}
