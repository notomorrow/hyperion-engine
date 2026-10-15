#ifndef HYP_GTAO_COMMON_HLSLI
#define HYP_GTAO_COMMON_HLSLI

// Ground truth ambient occlusion (Jimenez et al. 2016), following Intel's XeGTAO (MIT licence): https://github.com/GameTechDev/XeGTAO

#define GTAO_DEPTH_MIP_LEVELS 5

struct GTAOConstants
{
    Camera camera;
    float4x4 previousViewProjection; // kept by the pass, so it's never stale
    uint4 dimensions;  // xy = the view's, z = frame, w = 1 when last frame's history can be read
    float4 ndcToView;   // view xy = (uv * xy + zw) * view z
    float4 depthUnpack; // view z = x / (depth - y), zw = pixel size in uv
    float4 params0;     // x = radius, y = falloff range, z = sample distribution power, w = thin occluder compensation
    float4 params1;     // x = final value power, y = depth mip sampling offset, z = slices, w = steps per slice side
    float4 params2;     // x = denoise blur beta, y = screen radius in pixels at most, z = frames the history adds up at most, w = 1 on the last denoise pass
};

DECLARE_BUFFER_DYNAMIC(GTAO, CBuffer) cbuffer CBuffer
{
    GTAOConstants gtao;
};

#define GTAO_PI 3.1415926535897932384626433832795
#define GTAO_PI_HALF 1.5707963267948966192313216916398

bool GTAOIsSky(float viewZ)
{
    return viewZ >= gtao.camera.far * 0.999;
}

float GTAOViewDepth(float deviceDepth)
{
    if (deviceDepth >= 1.0)
    {
        return gtao.camera.far;
    }

    return min(gtao.depthUnpack.x / (deviceDepth - gtao.depthUnpack.y), gtao.camera.far);
}

float3 GTAOViewPosition(float2 uv, float viewZ)
{
    return float3((uv * gtao.ndcToView.xy + gtao.ndcToView.zw) * viewZ, viewZ);
}

float GTAOFastACos(float x)
{
    float res = -0.156583 * abs(x) + GTAO_PI_HALF;
    res *= sqrt(1.0 - abs(x));

    return x >= 0.0 ? res : GTAO_PI - res;
}

// slope aware: 1 where a neighbour continues the surface, 0 across a depth discontinuity
float4 GTAOCalculateEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ)
{
    float4 edgesLRTB = float4(leftZ, rightZ, topZ, bottomZ) - centerZ;

    const float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
    const float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
    const float4 edgesLRTBSlopeAdjusted = edgesLRTB + float4(slopeLR, -slopeLR, slopeTB, -slopeTB);

    edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));

    return saturate(1.25 - edgesLRTB / (centerZ * 0.011));
}

uint GTAOHilbertIndex(uint posX, uint posY)
{
    const uint width = 64u;

    uint index = 0u;

    for (uint level = width / 2u; level > 0u; level /= 2u)
    {
        const uint regionX = (posX & level) > 0u ? 1u : 0u;
        const uint regionY = (posY & level) > 0u ? 1u : 0u;

        index += level * level * ((3u * regionX) ^ regionY);

        if (regionY == 0u)
        {
            if (regionX == 1u)
            {
                posX = (width - 1u) - posX;
                posY = (width - 1u) - posY;
            }

            const uint temp = posX;
            posX = posY;
            posY = temp;
        }
    }

    return index;
}

// Hilbert curve ordered R2 sequence, offset each frame so the history sees new directions
float2 GTAOSpatioTemporalNoise(uint2 pixel, uint frame)
{
    uint index = GTAOHilbertIndex(pixel.x % 64u, pixel.y % 64u);
    index += 288u * (frame % 64u);

    return frac(0.5 + float(index) * float2(0.75487766624669276005, 0.5698402909980532659114));
}

#endif
