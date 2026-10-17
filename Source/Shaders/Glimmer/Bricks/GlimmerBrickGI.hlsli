#ifndef GLIMMER_BRICK_GI_HLSLI
#define GLIMMER_BRICK_GI_HLSLI

/// diffuse GI on the bricks: each tile of a card (its texel at GLIMMER_BRICK_CARD_FIRST_MIP) keeps GLIMMER_BRICK_GI_RAYS rays, traced once,
/// and gathers what they found each time its brick is lit
/// --
/// a slot is a word: kind in the top two bits
///     0: not traced, or the tile has no surface
///     1: a crossing: its index, and 6 bits of a hash of its first word to tell when its place was reused
///     2: the sky, possibly through canopy: the direction, octahedral 10 + 10 bits, and the transmittance, 8 bits
///     3: something past the bricks (ground, span solids): albedo times the transmittance to it, 5:6:5, and normal, octahedral 6 + 6 bits; bit 28 = dark (a back face)

// NOTE must match GlimmerBrickWorld.hpp
#define GLIMMER_BRICK_GI_RAYS 8
#define GLIMMER_BRICK_GI_TILES (GLIMMER_BRICK_CARD_SIDE * GLIMMER_BRICK_CARD_SIDE)

#define GLIMMER_BRICK_SLOT_NONE 0u
#define GLIMMER_BRICK_SLOT_CROSSING 1u
#define GLIMMER_BRICK_SLOT_SKY 2u
#define GLIMMER_BRICK_SLOT_OUTSIDE 3u

#define GLIMMER_BRICK_SLOT_DARK ((GLIMMER_BRICK_SLOT_OUTSIDE << 30u) | (1u << 28u))

uint GlimmerBrickSlotIndex(uint card, uint tile, uint ray)
{
    return (card * uint(GLIMMER_BRICK_GI_TILES) + tile) * uint(GLIMMER_BRICK_GI_RAYS) + ray;
}

uint GlimmerBrickSlotKind(uint slot)
{
    return slot >> 30u;
}

uint GlimmerBrickCrossingHash(uint firstWord)
{
    return (firstWord * 2654435761u) >> 26u;
}

uint GlimmerBrickEncodeCrossingSlot(uint crossing, uint firstWord)
{
    return (GLIMMER_BRICK_SLOT_CROSSING << 30u) | (GlimmerBrickCrossingHash(firstWord) << 24u) | (crossing & 0xFFFFFFu);
}

uint GlimmerBrickPackOctahedral(float3 n, uint bits)
{
    const float scale = float((1u << bits) - 1u);
    const uint2 q = uint2(saturate(GlimmerOctahedralEncode(n)) * scale + 0.5);

    return q.x | (q.y << bits);
}

float3 GlimmerBrickUnpackOctahedral(uint packed, uint bits)
{
    const uint mask = (1u << bits) - 1u;

    return normalize(GlimmerOctahedralDecode(float2(float(packed & mask), float((packed >> bits) & mask)) / float(mask)));
}

uint GlimmerBrickEncodeSkySlot(float3 direction, float transmittance)
{
    return (GLIMMER_BRICK_SLOT_SKY << 30u) | (uint(saturate(transmittance) * 255.0 + 0.5) << 20u) | GlimmerBrickPackOctahedral(direction, 10u);
}

uint GlimmerBrickEncodeOutsideSlot(float3 albedo, float3 normal)
{
    return (GLIMMER_BRICK_SLOT_OUTSIDE << 30u) | (GlimmerBrickPackOctahedral(normal, 6u) << 16u) | GlimmerBrickPackColor(albedo);
}

uint GlimmerBrickGIHash(uint x)
{
    x ^= x >> 16u;
    x *= 0x7FEB352Du;
    x ^= x >> 15u;
    x *= 0x846CA68Bu;
    x ^= x >> 16u;

    return x;
}

float GlimmerBrickGIHash01(uint x)
{
    return float(GlimmerBrickGIHash(x) >> 8u) * (1.0 / 16777216.0);
}

// ray `ray` of a tile's GLIMMER_BRICK_GI_RAYS: one stratum each of a cosine weighted hemisphere, turned by the tile's seed
float3 GlimmerBrickGIDirection(float3 N, uint ray, uint seed)
{
    const float u1 = (float(ray) + GlimmerBrickGIHash01(seed)) / float(GLIMMER_BRICK_GI_RAYS);
    const float u2 = frac(float(ray) * 0.6180339887 + GlimmerBrickGIHash01(seed * 7u + 1u));

    const float radius = sqrt(u1);
    const float phi = 2.0 * HYP_FMATH_PI * u2;
    const float3 local = float3(radius * cos(phi), radius * sin(phi), sqrt(max(1.0 - u1, 0.0)));

    const float signZ = N.z >= 0.0 ? 1.0 : -1.0;
    const float a = -1.0 / (signZ + N.z);
    const float b = N.x * N.y * a;
    const float3 T = float3(1.0 + signZ * N.x * N.x * a, signZ * b, -signZ * N.x);
    const float3 B = float3(b, signZ + N.y * N.y * a, -N.y);

    return normalize(T * local.x + B * local.y + N * local.z);
}

#ifdef GLIMMER_LIGHT_HLSLI

// along a direction, in the units GlimmerComposeRayLight gives an escaped ray
float3 GlimmerBrickSkyRadiance(GlimmerLightParams params, float3 direction)
{
    float basis[GLIMMER_LIGHT_TERMS];
    GlimmerLightBasis(direction, basis);

    float3 radiance = (float3)0.0;

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        radiance += params.directWeights[term].rgb * basis[term];
    }

    return max(radiance, (float3)0.0);
}

// irradiance / pi from the open sky on a surface facing N: the sky's terms convolved with the cosine lobe
float3 GlimmerBrickSkyIrradiance(GlimmerLightParams params, float3 N)
{
    float basis[GLIMMER_LIGHT_TERMS];
    GlimmerLightBasis(N, basis);

    float3 irradiance = (float3)0.0;

    [unroll]
    for (uint term = 0; term < GLIMMER_LIGHT_TERMS; term++)
    {
        const float lobe = term == 0u ? 1.0 : (term < 4u ? 2.0 / 3.0 : 0.25);

        irradiance += params.directWeights[term].rgb * (basis[term] * lobe);
    }

    return max(irradiance, (float3)0.0);
}

#endif

#endif
