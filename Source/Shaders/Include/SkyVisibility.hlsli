#ifndef HYP_SKY_VISIBILITY_HLSLI
#define HYP_SKY_VISIBILITY_HLSLI

struct SkyVisibilityCapture
{
    float4x4 viewProjMatrix;
    float4 params;
};

float EvaluateSkyVisibility(SkyVisibilityCapture skyVisibilityCapture, Texture2D skyVisibilityTexture, float3 positionWS, float3 N)
{
    const float strength = world_shader_data.sky_occlusion_params.x;

    if (strength <= 0.0 || skyVisibilityCapture.params.z == 0.0)
    {
        return 1.0;
    }

    const float worldExtent = skyVisibilityCapture.params.x;
    const float depthRange = skyVisibilityCapture.params.y;

    const float filterRadius = world_shader_data.sky_occlusion_params.y;
    const float baseBias = world_shader_data.sky_occlusion_params.z;

    // a sloped surface rises by this much across the filter, and would otherwise shadow itself
    const float slopeTangent = min(sqrt(saturate(1.0 - N.y * N.y)) / max(N.y, 0.15), 1.0) * saturate(N.y);

    const float3 samplePosition = positionWS + N * (baseBias + filterRadius * slopeTangent * 0.5);

    const float4 capturePosition = mul(skyVisibilityCapture.viewProjMatrix, float4(samplePosition, 1.0));
    const float3 captureNdc = capturePosition.xyz / capturePosition.w;

    if (any(abs(captureNdc.xy) > 1.0) || captureNdc.z < 0.0 || captureNdc.z > 1.0)
    {
        return 1.0;
    }

    const float2 captureUv = captureNdc.xy * float2(0.5, -0.5) + 0.5;
    const float sampleRadius = filterRadius / worldExtent;
    
    const float referenceDepth = captureNdc.z - (baseBias + filterRadius * slopeTangent) / depthRange;
    
    uint2 captureDimensions;
    skyVisibilityTexture.GetDimensions(captureDimensions.x, captureDimensions.y);

    const float softnessDepth = max(baseBias, 0.25) / depthRange;

    // fixed tent-weighted grid; nothing accumulates this over time, so a noise-rotated kernel shows up as per-pixel noise
    float openSum = 0.0;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            const float weight = float((2 - abs(x)) * (2 - abs(y))) / 16.0;
            const float2 offset = float2(x, y) * sampleRadius;

            const float2 texelPosition = (captureUv + offset) * float2(captureDimensions) - 0.5;
            const float2 texelFraction = frac(texelPosition);
            const float2 gatherUv = (floor(texelPosition) + 1.0) / float2(captureDimensions);

            const float4 occluderDepths = skyVisibilityTexture.GatherRed(sampler_nearest, gatherUv);
            const float4 open = saturate((occluderDepths - referenceDepth) / softnessDepth + 1.0);

            openSum += weight * lerp(lerp(open.w, open.z, texelFraction.x), lerp(open.x, open.y, texelFraction.x), texelFraction.y);
        }
    }

    const float2 edgeDistance = 1.0 - abs(captureNdc.xy);
    const float edgeFade = saturate(min(edgeDistance.x, edgeDistance.y) * 8.0);

    return lerp(1.0, openSum, strength * edgeFade);
}

#endif // HYP_SKY_VISIBILITY_HLSLI
