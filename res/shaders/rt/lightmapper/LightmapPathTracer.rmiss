#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#define PATHTRACER
#define LIGHTMAPPER
#include "../../include/rt/payload.inc"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    payload.emissive = vec4(-.0);
    payload.throughput = vec4(0.0);
    payload.entity_index = ~0u;
    payload.triangle_index = ~0u;
    payload.barycentric_coords = vec3(0.0);
    payload.distance = -1.0f;
    payload.normal = vec3(0.0f);
    payload.roughness = 0.0f;
}