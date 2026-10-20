#ifndef HYP_FOLIAGE_LIGHTING_HLSLI
#define HYP_FOLIAGE_LIGHTING_HLSLI

#define FOLIAGE_WRAP 0.5
#define FOLIAGE_TRANSMISSION_POWER 4.0
// a canopy is thousands of tiny leaves at random angles - sharp highlights off bent normals read as frost
#define FOLIAGE_MIN_ROUGHNESS 0.6
#define FOLIAGE_SPECULAR_FADE_NDOTV 0.5

float FoliageWrapDiffuse(float NdotL)
{
    return saturate((NdotL + FOLIAGE_WRAP) / ((1.0 + FOLIAGE_WRAP) * (1.0 + FOLIAGE_WRAP)));
}

float FoliageSpecularWeight(float3 N, float3 V)
{
    return smoothstep(0.0, FOLIAGE_SPECULAR_FADE_NDOTV, dot(N, V));
}

float3 FoliageTransmissionTint(float3 albedo)
{
    return albedo / max(max(albedo.r, max(albedo.g, albedo.b)), 0.001);
}

float FoliageTransmission(float3 V, float3 L, float transmission)
{
    return pow(saturate(dot(V, -L)), FOLIAGE_TRANSMISSION_POWER) * transmission;
}

#endif // HYP_FOLIAGE_LIGHTING_HLSLI
