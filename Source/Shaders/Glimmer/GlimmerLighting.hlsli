#ifndef GLIMMER_LIGHTING_HLSLI
#define GLIMMER_LIGHTING_HLSLI

#include "GlimmerLight.hlsli"

#define GLIMMER_MAX_ALBEDO 0.9

float3 GlimmerSkyIrradiance(float3 N)
{
    if (skyProbe.textureIndices == ~0u)
    {
        return (float3)0.0;
    }

    float shBands[9];
    ProjectSHBands(N, shBands);

    return max(EnvProbeSH(skyProbe, shBands), (float3)0.0) * skyProbe.world_position.w * world_shader_data.sky_light_params.x;
}

GlimmerLight GlimmerIndirect(float3 P, float3 N)
{
    GlimmerLight nearField;
    const float nearCoverage = SampleGlimmerProbesLight(GLIMMER_LIGHTING_PROBES, P + N * 0.1, N, nearField);

    GlimmerLight farField = GlimmerNoLight();
    float farCoverage = 0.0;

    [branch]
    if (nearCoverage < 0.999)
    {
        farCoverage = GlimmerSHSampleVolumeLight(GLIMMER_LIGHTING_SH, P, N, farField);
    }

    const float farWeight = farCoverage * (1.0 - nearCoverage);
    const float weight = nearCoverage + farWeight;

    // where nothing covers P, half of the distant light along N
    GlimmerLight light = GlimmerDistantLight(N, 0.5 * (1.0 - weight));

    GlimmerAddLight(light, nearField, (float3)nearCoverage);
    GlimmerAddLight(light, farField, (float3)farWeight);

    return light;
}

GlimmerLight GlimmerIndirectNearest(float3 P, float3 N)
{
    GlimmerLight light;

    if (SampleGlimmerNearestProbeLight(GLIMMER_LIGHTING_PROBES, P, N, light))
    {
        return light;
    }

    if (GlimmerSHSampleNearestLight(GLIMMER_LIGHTING_SH, P, N, 0.0, light))
    {
        return light;
    }

    return GlimmerDistantLight(N, 0.5);
}

// what a surface sends on of what it receives
GlimmerLight GlimmerShade(float3 albedo, GlimmerLight received)
{
    return GlimmerScaleLight(received, min(albedo, (float3)GLIMMER_MAX_ALBEDO));
}

GlimmerLight GlimmerShadeSurface(float3 P, float3 N, float3 albedo)
{
    return GlimmerShade(albedo, GlimmerIndirect(P, N));
}

#ifdef GLIMMER_RELIGHT_SAMPLING_HLSLI

bool GlimmerShadeRelitHit(float3 P, float3 albedo, float3 N, bool isGroundHit, uint level, out GlimmerLight outLight)
{
    outLight = GlimmerNoLight();

    GlimmerLight received;

    if (!GlimmerRelightCovers(isGroundHit, N)
        || !GlimmerSampleRelight(GLIMMER_LIGHTING_RELIGHT, GLIMMER_LIGHTING_GROUND, level, P.xz, isGroundHit ? GLIMMER_RELIGHT_GROUND : GLIMMER_RELIGHT_SPAN_TOP, received))
    {
        return false;
    }

    outLight = GlimmerShade(albedo, received);

    return true;
}

#endif

GlimmerLight GlimmerCanopyRadiance(float3 P, float3 weightedAlbedo)
{
    return GlimmerScaleLight(GlimmerIndirectNearest(P, float3(0.0, 1.0, 0.0)), weightedAlbedo);
}

#endif
