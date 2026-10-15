#include "../Include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GTAOCommon.hlsli"

// 3x3 blur that only crosses the edges the main pass found continuous

DECLARE_SRV(GTAO, InAO) Texture2D<float> InAO;
DECLARE_SRV(GTAO, InEdges) Texture2D<float4> InEdges;

DECLARE_UAV(GTAO, OutAO) RWTexture2D<float> OutAO;

int2 ClampCoord(int2 coord)
{
    return clamp(coord, int2(0, 0), int2(gtao.dimensions.xy) - 1);
}

float LoadAO(int2 coord)
{
    return InAO.Load(int3(ClampCoord(coord), 0));
}

float4 LoadEdges(int2 coord)
{
    return InEdges.Load(int3(ClampCoord(coord), 0));
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

    const bool isFinalPass = gtao.params2.w > 0.5;
    const float blurAmount = isFinalPass ? gtao.params2.x : gtao.params2.x / 5.0;
    const float diagWeight = 0.85 * 0.5;

    const float4 edgesL = LoadEdges(coord + int2(-1, 0));
    const float4 edgesR = LoadEdges(coord + int2(1, 0));
    const float4 edgesT = LoadEdges(coord + int2(0, -1));
    const float4 edgesB = LoadEdges(coord + int2(0, 1));

    // a left edge on the right pixel doesn't always match the right edge on the left one, so take both
    float4 edgesC = LoadEdges(coord) * float4(edgesL.y, edgesR.x, edgesT.w, edgesB.z);

    // lets a little in where 3 or 4 sides are edges, which cuts down on aliasing
    const float leakThreshold = 2.5;
    const float leakStrength = 0.5;
    const float edginess = (saturate(4.0 - leakThreshold - dot(edgesC, float4(1.0, 1.0, 1.0, 1.0))) / (4.0 - leakThreshold)) * leakStrength;
    edgesC = saturate(edgesC + edginess);

    const float weightTL = diagWeight * (edgesC.x * edgesL.z + edgesC.z * edgesT.x);
    const float weightTR = diagWeight * (edgesC.z * edgesT.y + edgesC.y * edgesR.z);
    const float weightBL = diagWeight * (edgesC.w * edgesB.x + edgesC.x * edgesL.w);
    const float weightBR = diagWeight * (edgesC.y * edgesR.w + edgesC.w * edgesB.y);

    float sumWeight = blurAmount;
    float sum = LoadAO(coord) * sumWeight;

    sum += LoadAO(coord + int2(-1, 0)) * edgesC.x;
    sum += LoadAO(coord + int2(1, 0)) * edgesC.y;
    sum += LoadAO(coord + int2(0, -1)) * edgesC.z;
    sum += LoadAO(coord + int2(0, 1)) * edgesC.w;
    sumWeight += dot(edgesC, float4(1.0, 1.0, 1.0, 1.0));

    sum += LoadAO(coord + int2(-1, -1)) * weightTL;
    sum += LoadAO(coord + int2(1, -1)) * weightTR;
    sum += LoadAO(coord + int2(-1, 1)) * weightBL;
    sum += LoadAO(coord + int2(1, 1)) * weightBR;
    sumWeight += weightTL + weightTR + weightBL + weightBR;

    OutAO[pixCoord] = sum / sumWeight;
}
