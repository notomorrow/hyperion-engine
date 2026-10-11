#ifndef GLIMMER_LIGHT_HLSLI
#define GLIMMER_LIGHT_HLSLI

#include "GlimmerCommon.hlsli"

// the SH terms the distant light (the sun and the sky) is in: 4 or 9
// NOTE must match GlimmerLightTerms in GlimmerHelpers.hpp
#define GLIMMER_LIGHT_TERMS 9

#define GLIMMER_LIGHT_REST GLIMMER_LIGHT_TERMS
#define GLIMMER_LIGHT_PARTS (GLIMMER_LIGHT_TERMS + 1)

// what a probe or voxel holds, each an L1: the bounce (r, g, b per part), then the direct (one per term)
#define GLIMMER_LIGHT_DIRECT (GLIMMER_LIGHT_PARTS * 3)
#define GLIMMER_LIGHT_ENTRIES (GLIMMER_LIGHT_DIRECT + GLIMMER_LIGHT_TERMS)

#define GLIMMER_LIGHT_PACKED_UINTS (GLIMMER_LIGHT_PARTS * 2)
#define GLIMMER_LIGHT_PACKED_UINTS_PADDED ((GLIMMER_LIGHT_PACKED_UINTS + 3) / 4 * 4)

// a probe ray: its hit distance, how much of the sky it escaped to, then its light
#define GLIMMER_RAY_UINTS (GLIMMER_LIGHT_PACKED_UINTS + 2)

// light as what each SH term of the distant light adds to it, then the rest (emissive, lightmaps) as it is
struct GlimmerLight
{
    float3 parts[GLIMMER_LIGHT_PARTS];
};

struct GlimmerLightParams
{
    float4 bounceWeights[GLIMMER_LIGHT_TERMS]; // rgb = the sun's and the sky's SH term
    float4 directWeights[GLIMMER_LIGHT_TERMS]; // rgb = the sky's alone: the sun's direct light isn't Glimmer's
};

GlimmerLight GlimmerNoLight()
{
    GlimmerLight light;

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        light.parts[part] = (float3)0.0;
    }

    return light;
}

GlimmerLight GlimmerRestLight(float3 radiance)
{
    GlimmerLight light = GlimmerNoLight();
    light.parts[GLIMMER_LIGHT_REST] = radiance;

    return light;
}

GlimmerLight GlimmerScaleLight(GlimmerLight light, float3 scale)
{
    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        light.parts[part] *= scale;
    }

    return light;
}

void GlimmerAddLight(inout GlimmerLight light, GlimmerLight other, float3 scale)
{
    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        light.parts[part] += other.parts[part] * scale;
    }
}

// the SH basis the distant light's terms are in (the sky probe's)
void GlimmerLightBasis(float3 direction, out float basis[GLIMMER_LIGHT_TERMS])
{
    basis[0] = 0.282095;
    basis[1] = 0.488603 * direction.y;
    basis[2] = 0.488603 * direction.z;
    basis[3] = 0.488603 * direction.x;

#if GLIMMER_LIGHT_TERMS > 4
    basis[4] = 1.092548 * direction.x * direction.y;
    basis[5] = 1.092548 * direction.y * direction.z;
    basis[6] = 0.315392 * (3.0 * direction.z * direction.z - 1.0);
    basis[7] = 1.092548 * direction.x * direction.z;
    basis[8] = 0.546274 * (direction.x * direction.x - direction.y * direction.y);
#endif
}

// distant light arriving unblocked along a direction
GlimmerLight GlimmerDistantLight(float3 direction, float scale)
{
    GlimmerLight light = GlimmerNoLight();

    float basis[GLIMMER_LIGHT_TERMS];
    GlimmerLightBasis(direction, basis);

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        light.parts[term] = (float3)(basis[term] * scale);
    }

    return light;
}

uint2 GlimmerPackLightPart(float3 value)
{
    const uint3 halves = f32tof16(clamp(value, -65000.0, 65000.0));

    return uint2(halves.x | (halves.y << 16), halves.z);
}

float3 GlimmerUnpackLightPart(uint2 packed)
{
    return f16tof32(uint3(packed.x & 0xFFFFu, packed.x >> 16, packed.y & 0xFFFFu));
}

float GlimmerEvaluateLinearL1(float4 sh, float3 N)
{
    return sh.x + dot(sh.yzw, N);
}

// what a surface facing N receives of what a probe or voxel holds: the bounce and the direct together
GlimmerLight GlimmerEvaluateStoredLight(float4 entries[GLIMMER_LIGHT_ENTRIES], float3 N)
{
    GlimmerLight light;

    // a term's response is signed, which the non-negative reconstruction can't take
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        light.parts[term] = float3(
            GlimmerEvaluateLinearL1(entries[term * 3u], N),
            GlimmerEvaluateLinearL1(entries[term * 3u + 1u], N),
            GlimmerEvaluateLinearL1(entries[term * 3u + 2u], N))
            + GlimmerEvaluateLinearL1(entries[GLIMMER_LIGHT_DIRECT + term], N);
    }

    light.parts[GLIMMER_LIGHT_REST] = GlimmerEvaluateL1(
        entries[GLIMMER_LIGHT_REST * 3u],
        entries[GLIMMER_LIGHT_REST * 3u + 1u],
        entries[GLIMMER_LIGHT_REST * 3u + 2u],
        N);

    return light;
}

// what a probe or voxel holds under the lighting of now, as r, g, b L1s
void GlimmerComposeStoredLight(GlimmerLightParams params, float4 entries[GLIMMER_LIGHT_ENTRIES], out float4 outComposed[3])
{
    [unroll]
    for (uint channel = 0; channel < 3; channel++)
    {
        outComposed[channel] = entries[GLIMMER_LIGHT_REST * 3u + channel];
    }

    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        const float3 bounceWeight = params.bounceWeights[term].rgb;
        const float3 directWeight = params.directWeights[term].rgb;

        const float4 direct = entries[GLIMMER_LIGHT_DIRECT + term];

        outComposed[0] += entries[term * 3u] * bounceWeight.r + direct * directWeight.r;
        outComposed[1] += entries[term * 3u + 1u] * bounceWeight.g + direct * directWeight.g;
        outComposed[2] += entries[term * 3u + 2u] * bounceWeight.b + direct * directWeight.b;
    }
}

// a ray's light under the lighting of now
float3 GlimmerComposeRayLight(GlimmerLightParams params, GlimmerLight light, float3 direction, float escape)
{
    float basis[GLIMMER_LIGHT_TERMS];
    GlimmerLightBasis(direction, basis);

    float3 radiance = light.parts[GLIMMER_LIGHT_REST];

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        radiance += light.parts[term] * params.bounceWeights[term].rgb + params.directWeights[term].rgb * (basis[term] * escape);
    }

    return radiance;
}

#endif
