#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Packing.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#include "../../Include/Scene.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../GlimmerCommon.hlsli"

#define DEBUG_VIEW_SHADED 1
#define DEBUG_VIEW_STEPS 2
#define DEBUG_VIEW_RECEIVER 3
#define DEBUG_VIEW_LIGHT 4
#define DEBUG_VIEW_LIGHT_COARSE 5

DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickTableBuffer) StructuredBuffer<uint> glimmerBrickTable;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickIndexBuffer) StructuredBuffer<uint> glimmerBrickIndex;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickCrossingsBuffer) StructuredBuffer<uint> glimmerBrickCrossings;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickMaterialBuffer) StructuredBuffer<uint> glimmerBrickMaterial;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickCountersBuffer) StructuredBuffer<uint> glimmerBrickCounters;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickRadianceBuffer) StructuredBuffer<uint> glimmerBrickRadiance;
DECLARE_SRV(GlimmerBrickDebug, GlimmerBrickCrossingLightBuffer) StructuredBuffer<uint> glimmerBrickCrossingLight;

#define GLIMMER_BRICKS_WITH_LIGHT
#include "GlimmerBricks.hlsli"

struct GlimmerBrickDebugConstants
{
    uint4 dimensionsMode;          // xy = output size, z = debug view
    float4 params;                 // x = max trace distance
    uint4 info;                    // x = pool bricks, y = pool pages, z = pool cards
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

#include "../GlimmerDebug.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint2 dimensions = constants.dimensionsMode.xy;
    const uint2 coord = dispatchThreadId.xy;

    if (any(coord >= dimensions))
    {
        return;
    }

    const uint mode = constants.dimensionsMode.z;
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

            result = ShadeHit(crossing.color, dot(crossing.normal, direction) > 0.0 ? -crossing.normal : crossing.normal);
        }
    }
    else if (mode == DEBUG_VIEW_STEPS)
    {
        result = HeatColor(float(brickHit.steps) / 256.0);
    }
    else if (mode == DEBUG_VIEW_LIGHT || mode == DEBUG_VIEW_LIGHT_COARSE)
    {
        if (didHitBricks)
        {
            int3 cell;
            uint axis;
            GlimmerLoadBrickPlace(brickHit.brick, cell, axis);

            const uint cardSlot = GlimmerBrickCardSlot(constants.bricks, brickHit.axis, brickHit.normal, brickHit.depth);

            // the crossing's own light, or its card's at the tiles
            const float4 light = mode == DEBUG_VIEW_LIGHT
                ? GlimmerBrickUnpackLight(glimmerBrickCrossingLight[brickHit.crossing])
                : GlimmerSampleBrickLight(constants.bricks, cell, brickHit.axis, cardSlot, cameraPosition + direction * brickHit.t, (float3)0.0, 4.0 * width);

            // magenta: the card has nothing there
            result = light.a > 1e-3 ? light.rgb / light.a : float3(1.0, 0.0, 1.0);
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
                    const bool isCurved = GlimmerBrickHasFlag(glimmerBrickMaterial[receiver.crossing], GLIMMER_BRICK_FLAG_CURVED);

                    result = (isCurved ? float3(1.0, 0.5, 0.0) : lerp(float3(0.0, 1.0, 0.0), HashColor(receiver.crossing), 0.35)) * grey;
                }
                else
                {
                    result = float3(1.0, 0.0, 0.0) * grey;
                }
            }
        }
    }

    // top left: how much of the brick, page and card pools is used, then whether the build ran out of bricks (red), crossings (yellow) or list nodes (blue)
    if (coord.y < 8u && coord.x < 256u)
    {
        const float used = coord.y < 3u
            ? 1.0 - float(glimmerBrickCounters[COUNTER_FREE_BRICKS]) / float(max(constants.info.x, 1u))
            : (coord.y < 6u
                ? 1.0 - float(glimmerBrickCounters[COUNTER_FREE_PAGES]) / float(max(constants.info.y, 1u))
                : 1.0 - float(glimmerBrickCounters[COUNTER_FREE_CARDS]) / float(max(constants.info.z, 1u)));

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
