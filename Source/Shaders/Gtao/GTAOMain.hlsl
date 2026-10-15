#include "../Include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GTAOCommon.hlsli"

DECLARE_SRV(GTAO, ViewDepthTexture) Texture2D<float> ViewDepthTexture;
DECLARE_SRV(GTAO, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;
DECLARE_SAMPLER(GTAO, SamplerPoint) SamplerState SamplerPoint;

DECLARE_UAV(GTAO, OutAO) RWTexture2D<float> OutAO;
DECLARE_UAV(GTAO, OutEdges) RWTexture2D<float4> OutEdges;

#include "../Include/Gbuffer.hlsli"

float LoadViewDepth(int2 coord)
{
    return ViewDepthTexture.Load(int3(clamp(coord, int2(0, 0), int2(gtao.dimensions.xy) - 1), 0));
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 pixCoord = dispatchThreadId.xy;

    if (any(pixCoord >= gtao.dimensions.xy))
    {
        return;
    }

    const int2 coord = int2(pixCoord);
    const float2 pixelSize = gtao.depthUnpack.zw;
    const float2 uv = (float2(pixCoord) + 0.5) * pixelSize;

    float viewZ = LoadViewDepth(coord);

    if (GTAOIsSky(viewZ))
    {
        OutAO[pixCoord] = 1.0;
        OutEdges[pixCoord] = float4(0.0, 0.0, 0.0, 0.0);

        return;
    }

    const float leftZ = LoadViewDepth(coord + int2(-1, 0));
    const float rightZ = LoadViewDepth(coord + int2(1, 0));
    const float topZ = LoadViewDepth(coord + int2(0, -1));
    const float bottomZ = LoadViewDepth(coord + int2(0, 1));

    OutEdges[pixCoord] = GTAOCalculateEdges(viewZ, leftZ, rightZ, topZ, bottomZ);

    // pulled towards the camera a touch so depth imprecision doesn't read as self occlusion
    viewZ *= 0.99999;

    const float3 pixCenterPos = GTAOViewPosition(uv, viewZ);
    const float3 viewVec = normalize(-pixCenterPos);

    const float3 normalWS = GBufferUnpackNormal(GBufferNormalsTexture.Load(int3(coord, 0)));
    float3 viewspaceNormal = normalize(mul(gtao.camera.view, float4(normalWS, 0.0)).xyz);

    // normal maps can tilt a normal past the silhouette, which the horizon math can't handle
    viewspaceNormal = normalize(viewspaceNormal + max(0.0, -dot(viewspaceNormal, viewVec)) * viewVec);

    const float effectRadius = gtao.params0.x;
    const float falloffRange = gtao.params0.y * effectRadius;
    const float sampleDistributionPower = gtao.params0.z;
    const float thinOccluderCompensation = gtao.params0.w;

    const float falloffFrom = effectRadius * (1.0 - gtao.params0.y);
    const float falloffMul = -1.0 / falloffRange;
    const float falloffAdd = falloffFrom / falloffRange + 1.0;

    const float finalValuePower = gtao.params1.x;
    const float depthMipSamplingOffset = gtao.params1.y;
    const float sliceCount = gtao.params1.z;
    const float stepsPerSlice = gtao.params1.w;

    const float2 noise = GTAOSpatioTemporalNoise(pixCoord, gtao.dimensions.z);
    const float noiseSlice = noise.x;
    const float noiseSample = noise.y;

    // offsets under about a pixel are pushed out, the center pixel tells us nothing
    const float pixelTooCloseThreshold = 1.3;

    const float viewspacePixelSize = viewZ * gtao.ndcToView.x * pixelSize.x;
    const float screenspaceRadius = min(effectRadius / viewspacePixelSize, gtao.params2.y);

    // fade out where the radius covers only a few pixels
    float visibility = saturate((10.0 - screenspaceRadius) / 100.0) * 0.5;

    const float minS = pixelTooCloseThreshold / screenspaceRadius;

    [loop]
    for (float sliceIndex = 0.0; sliceIndex < sliceCount; sliceIndex += 1.0)
    {
        const float sliceK = (sliceIndex + noiseSlice) / sliceCount;

        const float phi = sliceK * GTAO_PI;
        const float cosPhi = cos(phi);
        const float sinPhi = sin(phi);

        // screen y runs down, view y runs up
        const float2 omega = float2(cosPhi, -sinPhi) * screenspaceRadius;

        const float3 directionVec = float3(cosPhi, sinPhi, 0.0);
        const float3 orthoDirectionVec = directionVec - dot(directionVec, viewVec) * viewVec;

        const float3 axisVec = normalize(cross(orthoDirectionVec, viewVec));
        const float3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);

        const float signNorm = sign(dot(orthoDirectionVec, projectedNormalVec));

        float projectedNormalVecLength = length(projectedNormalVec);
        const float cosNorm = saturate(dot(projectedNormalVec, viewVec) / max(projectedNormalVecLength, 1e-6));

        const float n = signNorm * GTAOFastACos(cosNorm);

        const float lowHorizonCos0 = cos(n + GTAO_PI_HALF);
        const float lowHorizonCos1 = cos(n - GTAO_PI_HALF);

        float horizonCos0 = lowHorizonCos0;
        float horizonCos1 = lowHorizonCos1;

        [loop]
        for (float stepIndex = 0.0; stepIndex < stepsPerSlice; stepIndex += 1.0)
        {
            // R1 sequence along the slice
            const float stepBaseNoise = (sliceIndex + stepIndex * stepsPerSlice) * 0.6180339887498948482;
            const float stepNoise = frac(noiseSample + stepBaseNoise);

            float s = (stepIndex + stepNoise) / stepsPerSlice;
            s = pow(s, sampleDistributionPower);
            s += minS;

            float2 sampleOffset = s * omega;

            const float sampleOffsetLength = length(sampleOffset);
            const float mipLevel = clamp(log2(sampleOffsetLength) - depthMipSamplingOffset, 0.0, float(GTAO_DEPTH_MIP_LEVELS - 1));

            // snapped to texel centers, so the slope between pixels comes out right
            sampleOffset = round(sampleOffset) * pixelSize;

            const float2 sampleScreenPos0 = uv + sampleOffset;
            const float sampleZ0 = ViewDepthTexture.SampleLevel(SamplerPoint, sampleScreenPos0 * gtao.viewDepthScale.xy, mipLevel);
            const float3 samplePos0 = GTAOViewPosition(sampleScreenPos0, sampleZ0);

            const float2 sampleScreenPos1 = uv - sampleOffset;
            const float sampleZ1 = ViewDepthTexture.SampleLevel(SamplerPoint, sampleScreenPos1 * gtao.viewDepthScale.xy, mipLevel);
            const float3 samplePos1 = GTAOViewPosition(sampleScreenPos1, sampleZ1);

            const float3 sampleDelta0 = samplePos0 - pixCenterPos;
            const float3 sampleDelta1 = samplePos1 - pixCenterPos;
            const float sampleDist0 = length(sampleDelta0);
            const float sampleDist1 = length(sampleDelta1);

            const float3 sampleHorizonVec0 = sampleDelta0 / max(sampleDist0, 1e-6);
            const float3 sampleHorizonVec1 = sampleDelta1 / max(sampleDist1, 1e-6);

            // stretching the depth delta drops samples far behind the center sooner, so thin occluders cast less
            const float falloffBase0 = length(float3(sampleDelta0.xy, sampleDelta0.z * (1.0 + thinOccluderCompensation)));
            const float falloffBase1 = length(float3(sampleDelta1.xy, sampleDelta1.z * (1.0 + thinOccluderCompensation)));
            const float weight0 = saturate(falloffBase0 * falloffMul + falloffAdd);
            const float weight1 = saturate(falloffBase1 * falloffMul + falloffAdd);

            float shc0 = dot(sampleHorizonVec0, viewVec);
            float shc1 = dot(sampleHorizonVec1, viewVec);

            shc0 = lerp(lowHorizonCos0, shc0, weight0);
            shc1 = lerp(lowHorizonCos1, shc1, weight1);

            horizonCos0 = max(horizonCos0, shc0);
            horizonCos1 = max(horizonCos1, shc1);
        }

        // cosine weighting can't be fully undone for the projected normal's length, so nudge it towards 1
        projectedNormalVecLength = lerp(projectedNormalVecLength, 1.0, 0.05);

        const float h0 = -GTAOFastACos(horizonCos1);
        const float h1 = GTAOFastACos(horizonCos0);

        const float iarc0 = (cosNorm + 2.0 * h0 * sin(n) - cos(2.0 * h0 - n)) / 4.0;
        const float iarc1 = (cosNorm + 2.0 * h1 * sin(n) - cos(2.0 * h1 - n)) / 4.0;

        visibility += projectedNormalVecLength * (iarc0 + iarc1);
    }

    visibility /= sliceCount;
    visibility = pow(saturate(visibility), finalValuePower);
    visibility = max(0.03, visibility);

    OutAO[pixCoord] = visibility;
}
