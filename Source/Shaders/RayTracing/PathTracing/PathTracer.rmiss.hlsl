#define PATHTRACER

#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/RayTracing/Payload.hlsli"

[shader("miss")]
void MissMain(inout RayPayload payload)
{
    payload.emissive = float4(0.0, 0.0, 0.0, 0.0);
    payload.throughput = float4(0.0, 0.0, 0.0, 0.0);
    payload.distance = -1.0;
    payload.normal = float3(0.0, 0.0, 0.0);
    payload.roughness = 0.0;
}
