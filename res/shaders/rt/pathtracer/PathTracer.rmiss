#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#define PATHTRACER
#include "../../include/rt/payload.inc"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    payload.emissive = vec4(1.0, 0.0, 0.0, 1.0);
    payload.throughput = vec4(0.0);
    payload.distance = 1000000.0;
    payload.normal = vec3(0.0f);
    payload.roughness = 0.0f;
}