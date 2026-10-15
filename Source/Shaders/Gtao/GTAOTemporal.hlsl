#include "../Include/Defines.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "GTAOCommon.hlsli"

// adds the denoised AO onto last frame's, reprojected through the velocity buffer. History texels whose depth doesn't
// match where this pixel was last frame are dropped, and what's left is clamped to the pixel's neighbourhood

DECLARE_SRV(GTAO, InAO) Texture2D<float> InAO;
DECLARE_SRV(GTAO, ViewDepthTexture) Texture2D<float> ViewDepthTexture;
DECLARE_SRV(GTAO, GBufferVelocityTexture) Texture2D GBufferVelocityTexture;

// x = AO, y = view depth (0 for sky), z = frames added up
DECLARE_SRV(GTAO, HistoryTexture) Texture2D<float4> HistoryTexture;
DECLARE_UAV(GTAO, OutHistory) RWTexture2D<float4> OutHistory;

static const float HistoryDepthTolerance = 0.02;
static const float FreshHistoryFrames = 4.0;

int2 ClampCoord(int2 coord)
{
    return clamp(coord, int2(0, 0), int2(gtao.dimensions.xy) - 1);
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
    const float2 dimensions = float2(gtao.dimensions.xy);
    const float2 uv = (float2(pixCoord) + 0.5) / dimensions;

    const float viewZ = ViewDepthTexture.Load(int3(coord, 0));

    if (GTAOIsSky(viewZ))
    {
        OutHistory[pixCoord] = float4(1.0, 0.0, 0.0, 0.0);

        return;
    }

    const float center = InAO.Load(int3(coord, 0));

    // depth weighted so the neighbourhood doesn't take in what's across an edge
    float m1 = 0.0;
    float m2 = 0.0;
    float weightSum = 0.0;

    float neighbourZ[9];

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            const int2 neighbourCoord = ClampCoord(coord + int2(x, y));

            const float z = ViewDepthTexture.Load(int3(neighbourCoord, 0));
            const float ao = InAO.Load(int3(neighbourCoord, 0));

            neighbourZ[(y + 1) * 3 + (x + 1)] = z;

            const float weight = saturate(1.0 - abs(z - viewZ) / (viewZ * 0.05));

            m1 += ao * weight;
            m2 += ao * ao * weight;
            weightSum += weight;
        }
    }

    const float mean = m1 / weightSum;
    const float sigma = sqrt(max(m2 / weightSum - mean * mean, 0.0));

    // how far depth may drift between neighbouring texels on this surface, from the smaller one sided difference
    const float slopeX = min(abs(neighbourZ[3] - viewZ), abs(neighbourZ[5] - viewZ));
    const float slopeY = min(abs(neighbourZ[1] - viewZ), abs(neighbourZ[7] - viewZ));

    const float2 velocity = GBufferVelocityTexture.Load(int3(coord, 0)).rg;
    const float2 prevUV = uv - velocity;

    // where this pixel was last frame, assuming it hasn't moved by itself; the velocity covers things that have
    const float3 positionVS = GTAOViewPosition(uv, viewZ);
    const float4 positionWS = mul(gtao.camera.invViewMat, float4(positionVS, 1.0));
    const float expectedZ = mul(gtao.previousViewProjection, positionWS).w;

    const float depthTolerance = expectedZ * HistoryDepthTolerance + (slopeX + slopeY) * 1.5;

    float historyAO = 0.0;
    float historyFrames = 0.0;
    float historyWeight = 0.0;

    if (gtao.dimensions.w != 0u && all(prevUV > 0.0) && all(prevUV < 1.0))
    {
        const float2 historyPos = prevUV * dimensions - 0.5;
        const int2 historyBase = int2(floor(historyPos));
        const float2 f = historyPos - float2(historyBase);

        const float bilinearWeights[4] = {
            (1.0 - f.x) * (1.0 - f.y),
            f.x * (1.0 - f.y),
            (1.0 - f.x) * f.y,
            f.x * f.y
        };

        const int2 offsets[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };

        [unroll]
        for (int i = 0; i < 4; i++)
        {
            const int2 historyCoord = historyBase + offsets[i];

            if (any(historyCoord < 0) || any(historyCoord >= int2(gtao.dimensions.xy)))
            {
                continue;
            }

            const float4 history = HistoryTexture.Load(int3(historyCoord, 0));

            const bool isValid = history.y > 0.0 && abs(history.y - expectedZ) < depthTolerance;
            const float weight = isValid ? bilinearWeights[i] : 0.0;

            historyAO += history.x * weight;
            historyFrames += history.z * weight;
            historyWeight += weight;
        }
    }

    const float maxFrames = gtao.params2.z;

    float frames = 1.0;
    float result = mean;

    if (historyWeight > 0.01)
    {
        historyAO /= historyWeight;
        historyFrames /= historyWeight;

        frames = min(historyFrames + 1.0, maxFrames);

        // a fresh history leans on the neighbourhood, so a disocclusion isn't one frame of noise
        const float current = lerp(mean, center, saturate((frames - 1.0) / FreshHistoryFrames));

        // looser while still, where there's nothing to ghost and the noise should average out
        const float motionPixels = length(velocity * dimensions);
        const float clampScale = lerp(2.0, 1.0, saturate(motionPixels));
        const float clampExtent = sigma * clampScale + 0.01;

        const float clampedHistory = clamp(historyAO, mean - clampExtent, mean + clampExtent);

        result = lerp(clampedHistory, current, 1.0 / frames);
    }

    OutHistory[pixCoord] = float4(result, min(viewZ, 65000.0), frames, 0.0);
}
