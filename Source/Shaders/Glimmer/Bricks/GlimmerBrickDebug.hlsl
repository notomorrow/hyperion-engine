#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Packing.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../Include/RayTracing/BVH.hlsli"
#include "../SWRT/GlimmerSWRTCommon.hlsli"

#define DEBUG_VIEW_SHADED 1
#define DEBUG_VIEW_STEPS 2
#define DEBUG_VIEW_ERROR 3
#define DEBUG_VIEW_RECEIVER 4

DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickTableBuffer) StructuredBuffer<uint> glimmerBrickTable;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickIndexBuffer) StructuredBuffer<uint> glimmerBrickIndex;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickCrossingsBuffer) StructuredBuffer<uint> glimmerBrickCrossings;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickExtraBuffer) StructuredBuffer<uint> glimmerBrickExtra;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickCountersBuffer) StructuredBuffer<uint> glimmerBrickCounters;

#include "GlimmerBricks.hlsli"

struct GlimmerBrickDebugConstants
{
    uint4 dimensionsModeInstances; // xy = output size, z = debug view, w = number of instances
    float4 params;                 // x = max trace distance
    uint4 info;                    // x = pool bricks, y = pool pages
    GlimmerBrickParams bricks;
};

DECLARE_BUFFER_DYNAMIC(GlimmerBrickDebug, CBuffer) cbuffer CBuffer
{
    GlimmerBrickDebugConstants constants;
};

DECLARE_UAV(GlimmerBrickDebug, OutImage) RWTexture2D<float4> OutImage;

DECLARE_SRV(GlimmerBrickDebug, GBufferDepthTexture) Texture2D GBufferDepthTexture;
DECLARE_SRV(GlimmerBrickDebug, GBufferNormalsTexture) Texture2D GBufferNormalsTexture;

DECLARE_SRV_DYNAMIC(GlimmerBrickDebug, CamerasBuffer) StructuredBuffer<Camera> _cameras_buffer;
#define camera _cameras_buffer[0]

DECLARE_SRV(GlimmerBrickDebug, GlimmerTLASNodesBuffer) StructuredBuffer<BVHNode> glimmerTLASNodes;
DECLARE_SRV(GlimmerBrickDebug, GlimmerInstancesBuffer) StructuredBuffer<GlimmerInstance> glimmerInstances;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBLASNodesBuffer) StructuredBuffer<BVHBLASNode> glimmerBLASNodes;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBLASTrianglesBuffer) StructuredBuffer<uint> glimmerBLASTriangles;

#include "../SWRT/GlimmerSWRT.hlsli"

// NOTE must match GlimmerBrickBuild.hlsl
#define COUNTER_FREE_BRICKS 0
#define COUNTER_FAILED_BRICKS 3
#define COUNTER_DROPPED_COLUMNS 4
#define COUNTER_DROPPED_NODES 5
#define COUNTER_FREE_PAGES 6

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

    return t < 0.5 ? lerp(float3(0.05, 0.1, 0.45), float3(0.1, 0.85, 0.35), t * 2.0) : lerp(float3(0.1, 0.85, 0.35), float3(0.95, 0.15, 0.05), (t - 0.5) * 2.0);
}

float3 SkyColor(float3 direction)
{
    return lerp(float3(0.35, 0.4, 0.45), float3(0.15, 0.25, 0.45), saturate(direction.y));
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 dimensions = constants.dimensionsModeInstances.xy;
    const uint2 coord = dispatchThreadId.xy;

    if (any(coord >= dimensions))
    {
        return;
    }

    const uint mode = constants.dimensionsModeInstances.z;
    const float2 uv = (float2(coord) + 0.5) / float2(dimensions);

    const float3 cameraPosition = camera.position.xyz;
    const float3 farPosition = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, uv, 1.0).xyz;
    const float3 direction = normalize(farPosition - cameraPosition);

    const float maxDistance = constants.params.x;
    const float width = constants.bricks.params.z;

    GlimmerBrickHit brickHit;
    const bool didHitBricks = TraceGlimmerBricks(constants.bricks, cameraPosition, direction, 0.0, maxDistance, brickHit);

    float3 result = SkyColor(direction);

    if (mode == DEBUG_VIEW_SHADED)
    {
        if (didHitBricks)
        {
            const GlimmerBrickCrossing crossing = GlimmerLoadBrickCrossing(constants.bricks, brickHit.crossing);
            const float3 normal = dot(crossing.normal, direction) > 0.0 ? -crossing.normal : crossing.normal;

            result = crossing.color * (0.2 * (0.5 + 0.5 * normal.y) + 0.8 * saturate(dot(normal, DebugLightDirection)));
        }
    }
    else if (mode == DEBUG_VIEW_STEPS)
    {
        result = HeatColor(float(brickHit.steps) / 256.0);
    }
    else if (mode == DEBUG_VIEW_ERROR)
    {
        GlimmerSWRTStats stats = GlimmerMakeSWRTStats();
        GlimmerSWRTHit bvhHit;

        const bool didHitBVH = TraceGlimmerSWRT(cameraPosition, direction, 0.0, maxDistance, constants.dimensionsModeInstances.w, false, bvhHit, stats);

        const float3 bvhPosition = cameraPosition + direction * bvhHit.t;
        const bool isCovered = didHitBVH && GlimmerBrickCellInWindow(constants.bricks, int3(floor(bvhPosition * constants.bricks.params.y)));

        if (!didHitBVH && !didHitBricks)
        {
            result = float3(0.02, 0.02, 0.02);
        }
        else if (didHitBVH && !isCovered && !didHitBricks)
        {
            result = float3(0.25, 0.25, 0.25);
        }
        else if (!didHitBVH || brickHit.t < bvhHit.t - 4.0 * width)
        {
            // phantom
            result = float3(0.0, 0.0, 1.0);
        }
        else if (!didHitBricks || brickHit.t > bvhHit.t + 4.0 * width)
        {
            // leak
            result = float3(1.0, 0.0, 0.0);
        }
        else if (abs(brickHit.t - bvhHit.t) > width)
        {
            result = float3(1.0, 1.0, 0.0);
        }
        else
        {
            result = float3(0.0, 1.0, 0.0);
        }
    }
    else if (mode == DEBUG_VIEW_RECEIVER)
    {
        const float depth = GBufferDepthTexture.Load(int3(coord, 0)).r;

        if (depth < 1.0)
        {
            const float3 position = ReconstructWorldSpacePositionFromDepth(camera.invProjMat, camera.invViewMat, uv, depth).xyz;
            const float3 normal = normalize(DecodeNormal(GBufferNormalsTexture.Load(int3(coord, 0)).yzww));

            const float grey = 0.25 + 0.5 * saturate(dot(normal, DebugLightDirection) * 0.5 + 0.5);

            if (!GlimmerBrickCellInWindow(constants.bricks, int3(floor(position * constants.bricks.params.y))))
            {
                result = (float3)(grey * 0.4);
            }
            else
            {
                const GlimmerBrickReceiver receiver = GlimmerFindBrickReceiver(constants.bricks, position, normal);

                if (receiver.found)
                {
                    const uint flags = glimmerBrickExtra[2u * receiver.crossing] >> 29u;

                    result = lerp(float3(0.0, 1.0, 0.0), HashColor(receiver.crossing), 0.35) * grey;

                    if ((flags & GLIMMER_BRICK_FLAG_CURVED) != 0u)
                    {
                        result = float3(1.0, 0.5, 0.0) * grey;
                    }
                }
                else
                {
                    result = float3(1.0, 0.0, 0.0) * grey;
                }
            }
        }
    }

    // top left: how much of the brick and page pools is used, then whether the build ran out of bricks (red), crossings (yellow) or list nodes (blue)
    if (coord.y < 8u && coord.x < 256u)
    {
        const float used = coord.y < 4u
            ? 1.0 - float(glimmerBrickCounters[COUNTER_FREE_BRICKS]) / float(max(constants.info.x, 1u))
            : 1.0 - float(glimmerBrickCounters[COUNTER_FREE_PAGES]) / float(max(constants.info.y, 1u));

        result = float(coord.x) / 256.0 < used ? (float3)1.0 : (float3)0.0;
    }
    else if (coord.y < 16u && coord.x < 256u)
    {
        const uint dropped = coord.x < 85u ? glimmerBrickCounters[COUNTER_FAILED_BRICKS] : (coord.x < 170u ? glimmerBrickCounters[COUNTER_DROPPED_COLUMNS] : glimmerBrickCounters[COUNTER_DROPPED_NODES]);
        const float3 color = coord.x < 85u ? float3(1.0, 0.0, 0.0) : (coord.x < 170u ? float3(1.0, 1.0, 0.0) : float3(0.0, 0.0, 1.0));

        result = dropped != 0u ? color : (float3)0.0;
    }

    OutImage[coord] = float4(result, 1.0);
}
