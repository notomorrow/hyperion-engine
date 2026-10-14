#ifndef GLIMMER_DEBUG_HLSLI
#define GLIMMER_DEBUG_HLSLI

static const float3 DebugLightDirection = normalize(float3(0.45, 0.8, 0.35));

float3 HashColor(uint value)
{
    uint hash = value * 747796405u + 2891336453u;
    hash = ((hash >> ((hash >> 28u) + 4u)) ^ hash) * 277803737u;
    hash = (hash >> 22u) ^ hash;

    return float3(float(hash & 0xFFu), float((hash >> 8u) & 0xFFu), float((hash >> 16u) & 0xFFu)) / 255.0 * 0.8 + 0.2;
}

float3 HeatColor(float value)
{
    const float t = saturate(value);

    const float3 cold = float3(0.05, 0.1, 0.45);
    const float3 mid = float3(0.1, 0.85, 0.35);
    const float3 hot = float3(0.95, 0.15, 0.05);

    return t < 0.5 ? lerp(cold, mid, t * 2.0) : lerp(mid, float3(1.0, 0.85, 0.1), (t - 0.5) * 2.0) * (t > 0.9 ? lerp(1.0, hot / float3(1.0, 0.85, 0.1), (t - 0.9) * 10.0) : 1.0);
}

float3 ShadeHit(float3 albedo, float3 normal)
{
    const float lambert = saturate(dot(normal, DebugLightDirection));
    const float skyTerm = 0.5 + 0.5 * normal.y;

    return albedo * (0.2 * skyTerm + 0.8 * lambert);
}

float3 SkyColor(float3 direction)
{
    return lerp(float3(0.35, 0.4, 0.45), float3(0.15, 0.25, 0.45), saturate(direction.y));
}

#endif
