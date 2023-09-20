#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#define PATHTRACER
#include "../../include/rt/payload.inc"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    payload.color = vec3(1.0);
    payload.distance = 10000.0f;
    payload.normal = vec3(0.0f);
    payload.roughness = 0.0f;
}