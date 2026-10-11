#ifndef GLIMMER_LIGHT_HLSLI
#define GLIMMER_LIGHT_HLSLI

#include "GlimmerCommon.hlsli"

// NOTE must match GlimmerSunKeys and GlimmerSkyTerms in GlimmerHelpers.hpp
// sky terms: 1 has the sky as the rays see it in units of its brightness; 4 or 9 has what each SH term of the sky adds
#define GLIMMER_SUN_KEYS 2
#define GLIMMER_SKY_TERMS 9

#define GLIMMER_LIGHT_SKY GLIMMER_SUN_KEYS
#define GLIMMER_LIGHT_REST (GLIMMER_SUN_KEYS + GLIMMER_SKY_TERMS)
#define GLIMMER_LIGHT_PARTS (GLIMMER_LIGHT_REST + 1)

#define GLIMMER_LIGHT_PACKED_UINTS (GLIMMER_LIGHT_PARTS * 2)
#define GLIMMER_LIGHT_PACKED_UINTS_PADDED ((GLIMMER_LIGHT_PACKED_UINTS + 3) / 4 * 4)

// a probe ray: its hit distance, then its light
#define GLIMMER_RAY_UINTS (GLIMMER_LIGHT_PACKED_UINTS + 1)

// radiance by where its light started: per unit of each sun key's irradiance, per sky term, and the rest as it is
struct GlimmerLight
{
    float3 parts[GLIMMER_LIGHT_PARTS];
};

struct GlimmerLightParams
{
    float4 weights[GLIMMER_LIGHT_PARTS];    // rgb = what the part is scaled by where the light is put back together
    float4 sunDirections[GLIMMER_SUN_KEYS]; // xyz = toward the key's sun
};

struct GlimmerSunVisibility
{
    float keys[GLIMMER_SUN_KEYS];
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

float3 GlimmerCompose(GlimmerLightParams params, GlimmerLight light)
{
    float3 radiance = (float3)0.0;

    [unroll]
    for (uint part = 0; part < GLIMMER_LIGHT_PARTS; part++)
    {
        radiance += light.parts[part] * params.weights[part].rgb;
    }

    return radiance;
}

#if GLIMMER_SKY_TERMS > 1

// the SH basis the sky probe's terms are in
void GlimmerSkyBasis(float3 direction, out float basis[GLIMMER_SKY_TERMS])
{
    basis[0] = 0.282095;
    basis[1] = 0.488603 * direction.y;
    basis[2] = 0.488603 * direction.z;
    basis[3] = 0.488603 * direction.x;

#if GLIMMER_SKY_TERMS > 4
    basis[4] = 1.092548 * direction.x * direction.y;
    basis[5] = 1.092548 * direction.y * direction.z;
    basis[6] = 0.315392 * (3.0 * direction.z * direction.z - 1.0);
    basis[7] = 1.092548 * direction.x * direction.z;
    basis[8] = 0.546274 * (direction.x * direction.x - direction.y * direction.y);
#endif
}

#endif

GlimmerSunVisibility GlimmerNoSunVisibility()
{
    GlimmerSunVisibility visibility;

    [unroll]
    for (uint key = 0; key < GLIMMER_SUN_KEYS; key++)
    {
        visibility.keys[key] = 0.0;
    }

    return visibility;
}

// whether a key's sun is where an earlier key's is, so what was traced for that one can be taken for it
bool GlimmerSunKeyRepeats(GlimmerLightParams params, uint key, out uint outEarlierKey)
{
    outEarlierKey = 0u;

#if GLIMMER_SUN_KEYS > 1
    [unroll]
    for (uint earlierKey = 0; earlierKey < GLIMMER_SUN_KEYS - 1; earlierKey++)
    {
        if (earlierKey < key && dot(params.sunDirections[earlierKey].xyz, params.sunDirections[key].xyz) > 0.9999999)
        {
            outEarlierKey = earlierKey;

            return true;
        }
    }
#endif

    return false;
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

float3 GlimmerEvaluateLightPart(uint part, float4 shR, float4 shG, float4 shB, float3 N)
{
#if GLIMMER_SKY_TERMS > 1
    // a sky term's response is signed, which the non-negative reconstruction can't take
    if (part >= GLIMMER_LIGHT_SKY && part < GLIMMER_LIGHT_REST)
    {
        return float3(shR.x + dot(shR.yzw, N), shG.x + dot(shG.yzw, N), shB.x + dot(shB.yzw, N));
    }
#endif

    return GlimmerEvaluateL1(shR, shG, shB, N);
}

#endif
