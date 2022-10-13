#version 460
#extension GL_EXT_ray_tracing : require

#include "../../include/rt/payload.inc"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    payload.color = vec3(0.0);
    payload.distance = -1.0f;
    payload.normal = vec3(0.0f);
    payload.roughness = 0.0f;
}