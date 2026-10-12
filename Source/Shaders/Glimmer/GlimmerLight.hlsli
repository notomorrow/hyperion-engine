#ifndef GLIMMER_LIGHT_HLSLI
#define GLIMMER_LIGHT_HLSLI

#include "GlimmerCommon.hlsli"

#define GLIMMER_LIGHT_TERMS 9

#define GLIMMER_LIGHT_REST GLIMMER_LIGHT_TERMS
#define GLIMMER_LIGHT_PARTS (GLIMMER_LIGHT_TERMS + 1)

#define GLIMMER_LIGHT_FULL_TERMS 4
#define GLIMMER_LIGHT_SCALAR_TERMS (GLIMMER_LIGHT_TERMS - GLIMMER_LIGHT_FULL_TERMS)

#define GLIMMER_LIGHT_BOUNCE_SCALARS_ENTRY (GLIMMER_LIGHT_FULL_TERMS * 3)
#define GLIMMER_LIGHT_DIRECT_ENTRY (GLIMMER_LIGHT_BOUNCE_SCALARS_ENTRY + (GLIMMER_LIGHT_SCALAR_TERMS * 3 + 3) / 4)
#define GLIMMER_LIGHT_TRANSFER_ENTRIES (GLIMMER_LIGHT_DIRECT_ENTRY + GLIMMER_LIGHT_TERMS)
#define GLIMMER_LIGHT_REST_ENTRY GLIMMER_LIGHT_TRANSFER_ENTRIES
#define GLIMMER_LIGHT_ENTRIES (GLIMMER_LIGHT_REST_ENTRY + 3)

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

uint2 GlimmerPackLightEntry(float4 value)
{
    const uint4 halves = f32tof16(clamp(value, -65000.0, 65000.0));

    return uint2(halves.x | (halves.y << 16), halves.z | (halves.w << 16));
}

float4 GlimmerUnpackLightEntry(uint2 packed)
{
    return f16tof32(uint4(packed.x & 0xFFFFu, packed.x >> 16, packed.y & 0xFFFFu, packed.y >> 16));
}

#define GLIMMER_LIGHT_TRANSFER_RANGE 1.5

bool GlimmerIsScalarEntry(uint entry)
{
    return entry >= GLIMMER_LIGHT_BOUNCE_SCALARS_ENTRY && entry < GLIMMER_LIGHT_DIRECT_ENTRY;
}

// an L1 as 11 bits for its mean and 7 for each of the rest, four values an entry as 8 bits each
uint GlimmerPackTransferEntry(float4 value, bool isScalars)
{
    const float4 curved = sign(value) * sqrt(saturate(abs(value) * (1.0 / GLIMMER_LIGHT_TRANSFER_RANGE)));

    if (isScalars)
    {
        const uint4 bytes = uint4(int4(round(curved * 127.0))) & 0xFFu;

        return bytes.x | (bytes.y << 8) | (bytes.z << 16) | (bytes.w << 24);
    }

    const uint mean = uint(int(round(curved.x * 1023.0))) & 0x7FFu;
    const uint3 rest = uint3(int3(round(curved.yzw * 63.0))) & 0x7Fu;

    return mean | (rest.x << 11) | (rest.y << 18) | (rest.z << 25);
}

float4 GlimmerUnpackTransferEntry(uint packed, bool isScalars)
{
    const float4 curved = isScalars
            ? (float4(int4(packed << 24, packed << 16, packed << 8, packed) >> 24) * (1.0 / 127.0))
            : float4(float(int(packed << 21) >> 21) * (1.0 / 1023.0),
                float3(int3(packed << 14, packed << 7, packed) >> 25) * (1.0 / 63.0));

    return curved * abs(curved) * GLIMMER_LIGHT_TRANSFER_RANGE;
}

float GlimmerEvaluateLinearL1(float4 sh, float3 N)
{
    return sh.x + 0.5 * dot(sh.yzw, N);
}

float4 GlimmerGetBounceEntry(float4 entries[GLIMMER_LIGHT_ENTRIES], uint term, uint channel)
{
    if (term < GLIMMER_LIGHT_FULL_TERMS)
    {
        return entries[term * 3u + channel];
    }

    const uint scalar = (term - GLIMMER_LIGHT_FULL_TERMS) * 3u + channel;

    return float4(entries[GLIMMER_LIGHT_BOUNCE_SCALARS_ENTRY + scalar / 4u][scalar % 4u], 0.0, 0.0, 0.0);
}

void GlimmerSetBounceEntry(inout float4 entries[GLIMMER_LIGHT_ENTRIES], uint part, uint channel, float4 sh)
{
    if (part == GLIMMER_LIGHT_REST)
    {
        entries[GLIMMER_LIGHT_REST_ENTRY + channel] = sh;
    }
    else if (part < GLIMMER_LIGHT_FULL_TERMS)
    {
        entries[part * 3u + channel] = sh;
    }
    else
    {
        const uint scalar = (part - GLIMMER_LIGHT_FULL_TERMS) * 3u + channel;

        entries[GLIMMER_LIGHT_BOUNCE_SCALARS_ENTRY + scalar / 4u][scalar % 4u] = sh.x;
    }
}

GlimmerLight GlimmerEvaluateStoredLight(float4 entries[GLIMMER_LIGHT_ENTRIES], float3 N)
{
    GlimmerLight light;

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        light.parts[term] = float3(
            GlimmerEvaluateLinearL1(GlimmerGetBounceEntry(entries, term, 0u), N),
            GlimmerEvaluateLinearL1(GlimmerGetBounceEntry(entries, term, 1u), N),
            GlimmerEvaluateLinearL1(GlimmerGetBounceEntry(entries, term, 2u), N))
            + GlimmerEvaluateLinearL1(entries[GLIMMER_LIGHT_DIRECT_ENTRY + term], N);
    }

    light.parts[GLIMMER_LIGHT_REST] = GlimmerEvaluateL1(
        entries[GLIMMER_LIGHT_REST_ENTRY],
        entries[GLIMMER_LIGHT_REST_ENTRY + 1u],
        entries[GLIMMER_LIGHT_REST_ENTRY + 2u],
        N);

    return light;
}

void GlimmerComposeStoredLight(GlimmerLightParams params, float4 entries[GLIMMER_LIGHT_ENTRIES], out float4 outComposed[3])
{
    [unroll]
    for (uint channel = 0; channel < 3; channel++)
    {
        outComposed[channel] = entries[GLIMMER_LIGHT_REST_ENTRY + channel];
    }

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        const float3 bounceWeight = params.bounceWeights[term].rgb;
        const float3 directWeight = params.directWeights[term].rgb;

        const float4 direct = entries[GLIMMER_LIGHT_DIRECT_ENTRY + term];

        outComposed[0] += GlimmerGetBounceEntry(entries, term, 0u) * bounceWeight.r + direct * directWeight.r;
        outComposed[1] += GlimmerGetBounceEntry(entries, term, 1u) * bounceWeight.g + direct * directWeight.g;
        outComposed[2] += GlimmerGetBounceEntry(entries, term, 2u) * bounceWeight.b + direct * directWeight.b;
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
