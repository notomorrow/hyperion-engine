#ifndef GLIMMER_LIGHT_HLSLI
#define GLIMMER_LIGHT_HLSLI

#include "GlimmerCommon.hlsli"

#define GLIMMER_LIGHT_TERMS 9

#define GLIMMER_LIGHT_REST GLIMMER_LIGHT_TERMS
#define GLIMMER_LIGHT_PARTS (GLIMMER_LIGHT_TERMS + 1)

#define GLIMMER_LIGHT_DIRECT (GLIMMER_LIGHT_PARTS * 3)
#define GLIMMER_LIGHT_ENTRIES (GLIMMER_LIGHT_DIRECT + GLIMMER_LIGHT_TERMS)

#define GLIMMER_LIGHT_PACKED_UINTS (GLIMMER_LIGHT_PARTS * 2)
#define GLIMMER_LIGHT_PACKED_UINTS_PADDED ((GLIMMER_LIGHT_PACKED_UINTS + 3) / 4 * 4)

#define GLIMMER_RAY_UINTS (GLIMMER_LIGHT_PACKED_UINTS + 2)

struct GlimmerLight
{
    float3 parts[GLIMMER_LIGHT_PARTS];
};

struct GlimmerLightParams
{
    float4 bounceWeights[GLIMMER_LIGHT_TERMS]; // rgb = the sun's and the sky's SH term
    float4 directWeights[GLIMMER_LIGHT_TERMS]; // rgb = the sky's alone
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
    return sh.x + 0.5 * dot(sh.yzw, N);
}

GlimmerLight GlimmerEvaluateStoredLight(float4 entries[GLIMMER_LIGHT_ENTRIES], float3 N)
{
    GlimmerLight light;
    
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
