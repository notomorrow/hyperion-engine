#ifndef GLIMMER_RELIGHT_HLSLI
#define GLIMMER_RELIGHT_HLSLI

#include "GlimmerLight.hlsli"

#define GLIMMER_RELIGHT_GROUND 0u
#define GLIMMER_RELIGHT_SPAN_TOP 1u
#define GLIMMER_RELIGHT_SURFACES 2u

// per level: a layer per surface with the sun's visibility for each key (a is negative where there's no surface), then the
// light the surfaces receive at half the resolution, a part to a quadrant of a layer
#define GLIMMER_RELIGHT_INDIRECT_RESOLUTION (GLIMMER_GROUND_RESOLUTION / 2)
#define GLIMMER_RELIGHT_INDIRECT_LAYERS ((GLIMMER_RELIGHT_SURFACES * GLIMMER_LIGHT_PARTS + 3) / 4)
#define GLIMMER_RELIGHT_LAYERS (GLIMMER_RELIGHT_SURFACES + GLIMMER_RELIGHT_INDIRECT_LAYERS)

struct GlimmerRelightParams
{
    int4 levels[GLIMMER_GROUND_LEVELS]; // xy = absolute texel of the lit window's origin, z = 1 once all of it has been lit
};

struct GlimmerRelitSurface
{
    GlimmerLight indirect;
    GlimmerSunVisibility sunVisibility;
};

uint3 GlimmerRelightVisibilityTexel(int2 texel, uint level, uint surface)
{
    return uint3(GlimmerWrapGroundTexel(texel), level * GLIMMER_RELIGHT_LAYERS + surface);
}

uint3 GlimmerRelightIndirectTexel(int2 texel, uint level, uint surface, uint part)
{
    const uint index = surface * GLIMMER_LIGHT_PARTS + part;
    const uint quadrant = index & 3u;

    const uint2 block = uint2((texel >> 1) & (GLIMMER_RELIGHT_INDIRECT_RESOLUTION - 1));

    return uint3(
        block + uint2(quadrant & 1u, quadrant >> 1) * GLIMMER_RELIGHT_INDIRECT_RESOLUTION,
        level * GLIMMER_RELIGHT_LAYERS + GLIMMER_RELIGHT_SURFACES + (index >> 2));
}

// the texel of each 2x2 block that lights the block (the ground's windows start on even texels, so it's always in with its block)
bool GlimmerRelightLightsBlock(int2 texel)
{
    return all((texel & 1) == 0);
}

bool GlimmerRelightCovers(bool isGroundHit, float3 hitNormal)
{
    return isGroundHit || hitNormal.y > 0.5;
}

#endif

#if defined(GLIMMER_RELIGHT_WITH_SAMPLING) && !defined(GLIMMER_RELIGHT_SAMPLING_HLSLI)
#define GLIMMER_RELIGHT_SAMPLING_HLSLI

bool GlimmerSampleRelight(GlimmerRelightParams params, GlimmerGroundParams ground, uint level, float2 worldXZ, uint surface, out GlimmerRelitSurface outRelit)
{
    outRelit.indirect = GlimmerNoLight();
    outRelit.sunVisibility = GlimmerNoSunVisibility();

    const int4 litWindow = params.levels[level];

    if (litWindow.z == 0)
    {
        return false;
    }

    const int2 texel = int2(floor(worldXZ * ground.levels[level].params.y));

    if (any(texel < litWindow.xy) || any(texel >= litWindow.xy + GLIMMER_GROUND_RESOLUTION))
    {
        return false;
    }

    const float4 visibility = glimmerRelight.Load(int4(GlimmerRelightVisibilityTexel(texel, level, surface), 0));

    if (visibility.a < 0.0)
    {
        return false;
    }

    [unroll]
    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
    {
        outRelit.sunVisibility.keys[key] = visibility[key];
    }

    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        outRelit.indirect.parts[part] = glimmerRelight.Load(int4(GlimmerRelightIndirectTexel(texel, level, surface, part), 0)).rgb;
    }

    return true;
}

#endif
