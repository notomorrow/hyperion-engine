#include "../Include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GTAOCommon.hlsli"

DECLARE_SRV(GTAO, GBufferDepthTexture) Texture2D GBufferDepthTexture;

DECLARE_UAV(GTAO, OutDepthMip0) RWTexture2D<float> OutDepthMip0;
DECLARE_UAV(GTAO, OutDepthMip1) RWTexture2D<float> OutDepthMip1;
DECLARE_UAV(GTAO, OutDepthMip2) RWTexture2D<float> OutDepthMip2;
DECLARE_UAV(GTAO, OutDepthMip3) RWTexture2D<float> OutDepthMip3;
DECLARE_UAV(GTAO, OutDepthMip4) RWTexture2D<float> OutDepthMip4;

groupshared float g_scratchDepths[8][8];

// weighted towards the farthest depth, so thin foreground detail doesn't smear into what's behind it
float GTAODepthMipFilter(float depth0, float depth1, float depth2, float depth3)
{
    const float maxDepth = max(max(depth0, depth1), max(depth2, depth3));

    const float effectRadius = 0.75 * gtao.params0.x;
    const float falloffRange = gtao.params0.y * effectRadius;
    const float falloffFrom = effectRadius * (1.0 - gtao.params0.y);

    const float falloffMul = -1.0 / falloffRange;
    const float falloffAdd = falloffFrom / falloffRange + 1.0;

    const float weight0 = saturate((maxDepth - depth0) * falloffMul + falloffAdd);
    const float weight1 = saturate((maxDepth - depth1) * falloffMul + falloffAdd);
    const float weight2 = saturate((maxDepth - depth2) * falloffMul + falloffAdd);
    const float weight3 = saturate((maxDepth - depth3) * falloffMul + falloffAdd);

    const float weightSum = weight0 + weight1 + weight2 + weight3;

    return (weight0 * depth0 + weight1 * depth1 + weight2 * depth2 + weight3 * depth3) / weightSum;
}

float LoadViewDepth(uint2 coord)
{
    return GTAOViewDepth(GBufferDepthTexture.Load(int3(min(coord, gtao.dimensions.xy - 1u), 0)).r);
}

bool IsInMip(uint mipLevel, uint2 coord)
{
    const uint2 mipDimensions = max(gtao.dimensions.xy >> mipLevel, uint2(1u, 1u));

    return all(coord < mipDimensions);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID, uint3 groupThreadId : SV_GroupThreadID)
{
    const uint2 baseCoord = dispatchThreadId.xy;
    const uint2 pixCoord = baseCoord * 2u;

    const float depth0 = LoadViewDepth(pixCoord + uint2(0u, 0u));
    const float depth1 = LoadViewDepth(pixCoord + uint2(1u, 0u));
    const float depth2 = LoadViewDepth(pixCoord + uint2(0u, 1u));
    const float depth3 = LoadViewDepth(pixCoord + uint2(1u, 1u));

    if (IsInMip(0u, pixCoord + uint2(0u, 0u)))
    {
        OutDepthMip0[pixCoord + uint2(0u, 0u)] = depth0;
    }

    if (IsInMip(0u, pixCoord + uint2(1u, 0u)))
    {
        OutDepthMip0[pixCoord + uint2(1u, 0u)] = depth1;
    }

    if (IsInMip(0u, pixCoord + uint2(0u, 1u)))
    {
        OutDepthMip0[pixCoord + uint2(0u, 1u)] = depth2;
    }

    if (IsInMip(0u, pixCoord + uint2(1u, 1u)))
    {
        OutDepthMip0[pixCoord + uint2(1u, 1u)] = depth3;
    }

    const float depthMip1 = GTAODepthMipFilter(depth0, depth1, depth2, depth3);

    if (IsInMip(1u, baseCoord))
    {
        OutDepthMip1[baseCoord] = depthMip1;
    }

    g_scratchDepths[groupThreadId.x][groupThreadId.y] = depthMip1;

    GroupMemoryBarrierWithGroupSync();

    [branch]
    if (all((groupThreadId.xy % 2u) == 0u))
    {
        const float inTL = g_scratchDepths[groupThreadId.x + 0][groupThreadId.y + 0];
        const float inTR = g_scratchDepths[groupThreadId.x + 1][groupThreadId.y + 0];
        const float inBL = g_scratchDepths[groupThreadId.x + 0][groupThreadId.y + 1];
        const float inBR = g_scratchDepths[groupThreadId.x + 1][groupThreadId.y + 1];

        const float depthMip2 = GTAODepthMipFilter(inTL, inTR, inBL, inBR);

        if (IsInMip(2u, baseCoord / 2u))
        {
            OutDepthMip2[baseCoord / 2u] = depthMip2;
        }

        g_scratchDepths[groupThreadId.x][groupThreadId.y] = depthMip2;
    }

    GroupMemoryBarrierWithGroupSync();

    [branch]
    if (all((groupThreadId.xy % 4u) == 0u))
    {
        const float inTL = g_scratchDepths[groupThreadId.x + 0][groupThreadId.y + 0];
        const float inTR = g_scratchDepths[groupThreadId.x + 2][groupThreadId.y + 0];
        const float inBL = g_scratchDepths[groupThreadId.x + 0][groupThreadId.y + 2];
        const float inBR = g_scratchDepths[groupThreadId.x + 2][groupThreadId.y + 2];

        const float depthMip3 = GTAODepthMipFilter(inTL, inTR, inBL, inBR);

        if (IsInMip(3u, baseCoord / 4u))
        {
            OutDepthMip3[baseCoord / 4u] = depthMip3;
        }

        g_scratchDepths[groupThreadId.x][groupThreadId.y] = depthMip3;
    }

    GroupMemoryBarrierWithGroupSync();

    [branch]
    if (all((groupThreadId.xy % 8u) == 0u))
    {
        const float inTL = g_scratchDepths[groupThreadId.x + 0][groupThreadId.y + 0];
        const float inTR = g_scratchDepths[groupThreadId.x + 4][groupThreadId.y + 0];
        const float inBL = g_scratchDepths[groupThreadId.x + 0][groupThreadId.y + 4];
        const float inBR = g_scratchDepths[groupThreadId.x + 4][groupThreadId.y + 4];

        const float depthMip4 = GTAODepthMipFilter(inTL, inTR, inBL, inBR);

        if (IsInMip(4u, baseCoord / 8u))
        {
            OutDepthMip4[baseCoord / 8u] = depthMip4;
        }
    }
}
