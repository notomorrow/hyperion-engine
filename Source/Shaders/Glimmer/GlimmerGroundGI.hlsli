#ifndef GLIMMER_GROUND_GI_HLSLI
#define GLIMMER_GROUND_GI_HLSLI

#include "GlimmerCommon.hlsli"
#include "GlimmerRelight.hlsli"

/// brick GI on the ground: each texel of the ground clipmap's levels is a surface with slots, as a card's tile is,
/// what arrives at it gathered from them, and what it sends on lit with the sun behind a shadow ray
/// --
/// glimmerGroundGI holds, per level, per texel of the window (wrapped): the slots, then what arrives, then what is sent
/// the lit window of each level is a GlimmerRelightParams (xy = origin texel, z = 1 once lit)
/// --
/// expects glimmerGroundGI to be declared

// NOTE must match GlimmerGroundGI.hpp
#define GLIMMER_GROUND_GI_RAYS 8
#define GLIMMER_GROUND_GI_TEXELS (GLIMMER_GROUND_RESOLUTION * GLIMMER_GROUND_RESOLUTION * GLIMMER_GROUND_LEVELS)

#define GLIMMER_GROUND_GI_ARRIVING (GLIMMER_GROUND_GI_TEXELS * GLIMMER_GROUND_GI_RAYS)
#define GLIMMER_GROUND_GI_SENT (GLIMMER_GROUND_GI_ARRIVING + GLIMMER_GROUND_GI_TEXELS)

// the canopy over a texel, where it has one: what arrives at its top from above and at its bottom from below (without the sun), and how much sun reaches its top (1 + 65534 x the share, so that 0 is none)
#define GLIMMER_GROUND_GI_CANOPY_TOP (GLIMMER_GROUND_GI_SENT + GLIMMER_GROUND_GI_TEXELS)
#define GLIMMER_GROUND_GI_CANOPY_BOTTOM (GLIMMER_GROUND_GI_CANOPY_TOP + GLIMMER_GROUND_GI_TEXELS)
#define GLIMMER_GROUND_GI_CANOPY_SUN (GLIMMER_GROUND_GI_CANOPY_BOTTOM + GLIMMER_GROUND_GI_TEXELS)

#define GLIMMER_GROUND_GI_CANOPY_RAYS 4

uint GlimmerGroundGITexelIndex(uint level, int2 texel)
{
    const uint2 wrapped = GlimmerWrapGroundTexel(texel);

    return (level * uint(GLIMMER_GROUND_RESOLUTION) + wrapped.y) * uint(GLIMMER_GROUND_RESOLUTION) + wrapped.x;
}

#ifndef GLIMMER_GROUND_GI_NO_SAMPLING

// one level of the ground cache at xz, bilinear over the texels that have something there; false outside its lit window
bool GlimmerSampleGroundGILevel(GlimmerRelightParams lit, GlimmerGroundParams ground, uint level, float2 xz, uint section, out float3 outLight)
{
    outLight = (float3)0.0;

    const int4 window = lit.levels[level];

    if (window.z == 0)
    {
        return false;
    }

    const float2 coord = xz * ground.levels[level].params.y - 0.5;
    const int2 texel0 = int2(floor(coord));

    if (any(texel0 < window.xy) || any(texel0 + 1 >= window.xy + GLIMMER_GROUND_RESOLUTION))
    {
        return false;
    }

    const float2 fraction = coord - float2(texel0);

    float4 sum = (float4)0.0;

    [unroll]
    for (uint tap = 0; tap < 4u; tap++)
    {
        const int2 offset = int2(tap & 1u, tap >> 1u);
        const uint packed = glimmerGroundGI[section + GlimmerGroundGITexelIndex(level, texel0 + offset)];

        if (packed == 0u)
        {
            continue;
        }

        const float2 weights = lerp(1.0 - fraction, fraction, float2(offset));
        const float weight = weights.x * weights.y;

        // the sun section is a plain fraction
        sum += (section == GLIMMER_GROUND_GI_CANOPY_SUN ? float4((float3)(float(packed - 1u) / 65534.0), 1.0) : float4(GlimmerBrickUnpackLight(packed).rgb, 1.0)) * weight;
    }

    if (sum.a <= 1e-4)
    {
        return false;
    }

    outLight = sum.rgb / sum.a;

    return true;
}

// light of the ground cache at xz, from the finest lit level that holds it: what arrives (GLIMMER_GROUND_GI_ARRIVING) or what is sent (GLIMMER_GROUND_GI_SENT)
bool GlimmerSampleGroundGI(GlimmerRelightParams lit, GlimmerGroundParams ground, float2 xz, uint section, out float3 outLight)
{
    outLight = (float3)0.0;

    [loop]
    for (uint level = 0; level < GLIMMER_GROUND_LEVELS; level++)
    {
        if (GlimmerSampleGroundGILevel(lit, ground, level, xz, section, outLight))
        {
            return true;
        }
    }

    return false;
}

#ifdef GLIMMER_HEIGHTFIELD_HLSLI

// metres above the canopy's top or below its bottom a point still counts as in it
#define GLIMMER_CANOPY_MARGIN 1.0

// metres of leaves light comes through sideways at least: crowns stand apart, so a leaf sees past its own crown's edge
// as well as down through all the leaves above it
#define GLIMMER_CANOPY_LATERAL_PATH 1.0

// irradiance / pi in the canopy over xz at height P.y, facing N (zero for leaves lit from all round, as the in-scatter is):
// what arrives at its top and bottom through the leaves above and below, and the sun on its top through the leaves above
bool GlimmerSampleCanopyGI(
    GlimmerRelightParams lit,
    GlimmerGroundParams ground,
    GlimmerSpanParams spans,
    float3 P,
    float3 N,
    float foliageExtinction,
    float3 toSun,
    float3 sunIrradiance,
    out float3 outIrradiance)
{
    outIrradiance = (float3)0.0;

    [loop]
    for (uint level = 0; level < GLIMMER_GROUND_LEVELS; level++)
    {
        GlimmerSpanSample spanSample;

        if (lit.levels[level].z == 0 || !GlimmerSampleSpans(spans, level, P.xz, spanSample))
        {
            continue;
        }

        if (spanSample.canopyBins == 0u)
        {
            return false;
        }

        const float bottom = spanSample.canopyMin;
        const float top = bottom + GlimmerSpanHeight(spanSample.canopyMin, spanSample.canopyMax);

        if (P.y > top + GLIMMER_CANOPY_MARGIN || P.y < bottom - GLIMMER_CANOPY_MARGIN)
        {
            return false;
        }

        float3 fromTop;
        float3 fromBottom;
        float3 sunVisibility;

        if (!GlimmerSampleGroundGILevel(lit, ground, level, P.xz, GLIMMER_GROUND_GI_CANOPY_TOP, fromTop)
            || !GlimmerSampleGroundGILevel(lit, ground, level, P.xz, GLIMMER_GROUND_GI_CANOPY_BOTTOM, fromBottom)
            || !GlimmerSampleGroundGILevel(lit, ground, level, P.xz, GLIMMER_GROUND_GI_CANOPY_SUN, sunVisibility))
        {
            return false;
        }

        const float y = clamp(P.y, bottom, top);
        const float densityScale = GLIMMER_LEAF_PROJECTION * foliageExtinction;

        const float above = GlimmerCrossSpan(y, top, top - y, spanSample.canopyMin, spanSample.canopyMax, spanSample.canopyBins, spanSample.canopyBands, densityScale, 0.5, GLIMMER_HEIGHT_UNBOUNDED).opticalDepth;
        const float below = GlimmerCrossSpan(bottom, y, y - bottom, spanSample.canopyMin, spanSample.canopyMax, spanSample.canopyBins, spanSample.canopyBands, densityScale, 0.5, GLIMMER_HEIGHT_UNBOUNDED).opticalDepth;

        const uint band = uint(GlimmerSpanBin(y, spanSample.canopyMin, spanSample.canopyMax)) / uint(GLIMMER_SPAN_BINS_PER_BAND);
        const float density = GlimmerSpanBandDensity(spanSample.canopyMin, spanSample.canopyMax, spanSample.canopyBins, spanSample.canopyBands, band, densityScale, 0.5);
        const float sideways = exp(-density * GLIMMER_CANOPY_LATERAL_PATH);

        const bool hasFacing = dot(N, N) > 0.0;
        const float upward = hasFacing ? saturate(0.5 + 0.5 * N.y) : 0.5;
        const float facingSun = hasFacing ? max(dot(N, toSun), 0.0) : 0.5;

        const float sunThrough = max(exp(-above / max(toSun.y, 0.2)), sideways);

        outIrradiance = fromTop * (max(exp(-above), sideways) * upward)
            + fromBottom * (max(exp(-below), sideways) * (1.0 - upward))
            + sunIrradiance * (sunVisibility.x * sunThrough * facingSun / HYP_FMATH_PI);

        return true;
    }

    return false;
}

#endif

#endif

#endif
