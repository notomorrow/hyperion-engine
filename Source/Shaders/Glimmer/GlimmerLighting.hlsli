#ifndef GLIMMER_LIGHTING_HLSLI
#define GLIMMER_LIGHTING_HLSLI

#include "GlimmerLight.hlsli"

#define GLIMMER_MAX_ALBEDO 0.9

float GlimmerLuminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

#if GLIMMER_SKY_TERMS > 1

// the sky along a direction, as the sky's part of the light: what each of its SH terms adds along it
GlimmerLight GlimmerSkyLight(float3 direction, float scale)
{
    GlimmerLight light = GlimmerNoLight();

    float basis[GLIMMER_SKY_TERMS];
    GlimmerSkyBasis(direction, basis);

    [unroll]
    for (uint term = 0; term < GLIMMER_SKY_TERMS; term++)
    {
        light.parts[GLIMMER_LIGHT_SKY + term] = (float3)(basis[term] * scale);
    }

    return light;
}

#else

#define GLIMMER_SKY_MIP 3.0

float3 GlimmerSkyRadiance(float3 direction)
{
    const GlimmerSkyParams sky = GLIMMER_LIGHTING_SKY;

    if (sky.info.x == 0xFFFFu || sky.info.x == 0xFFFFFFFFu)
    {
        return (float3)0.0;
    }

    float3 radiance = envProbesColorTexture.SampleLevel(glimmerMaterialSampler, float4(direction, float(sky.info.x)), GLIMMER_SKY_MIP).rgb;
    radiance *= sky.params.x * world_shader_data.sky_light_params.x;

    const float luminance = GlimmerLuminance(radiance);

    return luminance > sky.params.y ? radiance * (sky.params.y / luminance) : radiance;
}

// the sky along a direction, as the sky's part of the light: in units of the sky's brightness
GlimmerLight GlimmerSkyLight(float3 direction, float scale)
{
    GlimmerLight light = GlimmerNoLight();

    const float3 skyWeight = GLIMMER_LIGHTING_LIGHT.weights[GLIMMER_LIGHT_SKY].rgb;

    light.parts[GLIMMER_LIGHT_SKY] = select(skyWeight > 0.0, GlimmerSkyRadiance(direction) / max(skyWeight, 1e-6), (float3)0.0) * scale;

    return light;
}

#endif

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

float3 GlimmerSunDirection(uint key)
{
    return GLIMMER_LIGHTING_LIGHT.sunDirections[key].xyz;
}

float GlimmerCloudShadow(float3 P, float3 L)
{
    return GetCloudShadow(cloudWeatherMapTexture, cloudShadowMapTexture, glimmerMaterialSampler, cloudVolume, cloudWeatherMap, cloudShadowMap, P, L);
}

bool GlimmerFacesSun(float3 N, float3 L)
{
    return dot(N, L) > 0.0 && L.y > -0.1;
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
        farCoverage = GlimmerSHSampleVolumeLight(GLIMMER_LIGHTING_SH, P + N * 0.05, N, farField);
    }

    const float farWeight = farCoverage * (1.0 - nearCoverage);
    const float weight = nearCoverage + farWeight;

    GlimmerLight light = GlimmerSkyLight(N, 0.5 * (1.0 - weight));

    GlimmerAddLight(light, nearField, (float3)nearCoverage);
    GlimmerAddLight(light, farField, (float3)farWeight);

    return light;
}

// GlimmerIndirect from the nearest probe or voxel alone
GlimmerLight GlimmerIndirectNearest(float3 P, float3 N)
{
    GlimmerLight light;

    if (SampleGlimmerNearestProbeLight(GLIMMER_LIGHTING_PROBES, P, N, light))
    {
        return light;
    }

    if (GlimmerSHSampleNearestLight(GLIMMER_LIGHTING_SH, P, N, light))
    {
        return light;
    }

    return GlimmerSkyLight(N, 0.5);
}

GlimmerLight GlimmerShade(float3 P, float3 N, float3 albedo, GlimmerSunVisibility sunVisibility, GlimmerLight indirect)
{
    albedo = min(albedo, (float3)GLIMMER_MAX_ALBEDO);

    GlimmerLight light = indirect;

    [unroll]
    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
    {
        const float3 L = GlimmerSunDirection(key);

        if (GlimmerFacesSun(N, L) && sunVisibility.keys[key] > 0.0)
        {
            light.parts[key] += dot(N, L) * HYP_FMATH_ONE_OVER_PI * sunVisibility.keys[key] * GlimmerCloudShadow(P, L);
        }
    }

    return GlimmerScaleLight(light, albedo);
}

GlimmerLight GlimmerShadeSurface(float3 P, float3 N, float3 albedo, GlimmerSunVisibility sunVisibility)
{
    return GlimmerShade(P, N, albedo, sunVisibility, GlimmerIndirect(P, N));
}

#ifdef GLIMMER_RELIGHT_SAMPLING_HLSLI

bool GlimmerShadeRelitHit(float3 P, float3 albedo, float3 N, bool isGroundHit, uint level, out GlimmerLight outLight)
{
    outLight = GlimmerNoLight();

    GlimmerRelitSurface relit;

    if (!GlimmerRelightCovers(isGroundHit, N)
        || !GlimmerSampleRelight(GLIMMER_LIGHTING_RELIGHT, GLIMMER_LIGHTING_GROUND, level, P.xz, isGroundHit ? GLIMMER_RELIGHT_GROUND : GLIMMER_RELIGHT_SPAN_TOP, relit))
    {
        return false;
    }

    outLight = GlimmerShade(P, N, albedo, relit.sunVisibility, relit.indirect);

    return true;
}

#endif

static GlimmerLight g_canopySurroundings;
static bool g_hasCanopySurroundings = false;

GlimmerLight GlimmerCanopyRadiance(float3 P, float3 albedo, float depthBelowTop, float extinction)
{
    if (!g_hasCanopySurroundings)
    {
        g_canopySurroundings = GlimmerIndirectNearest(P, float3(0.0, 1.0, 0.0));
        g_hasCanopySurroundings = true;
    }

    GlimmerLight light = g_canopySurroundings;

    [unroll]
    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
    {
        const float3 L = GlimmerSunDirection(key);

        if (L.y > 0.0)
        {
            const float sunTransmittance = exp(-extinction * depthBelowTop / max(L.y, 0.15)) * GlimmerCloudShadow(P, L);

            light.parts[key] += sunTransmittance * (0.5 * HYP_FMATH_ONE_OVER_PI);
        }
    }

    return GlimmerScaleLight(light, min(albedo, (float3)GLIMMER_MAX_ALBEDO));
}

#endif
