#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#define PATHTRACER
#include "../../include/rt/payload.inc"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    vec3 sky_color = vec3(1.0);

    payload.color = sky_color;
    payload.emissive = sky_color * 100.0; // fake sky light
    payload.throughput = vec3(0.0);
    payload.distance = -1.0f;
    payload.normal = vec3(0.0f);
    payload.roughness = 0.0f;
}