#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#include "../include/rt/payload.inc"

layout(location = 0) rayPayloadInEXT RayProbePayload payload;

void main()
{
    // View-independent background gradient to simulate a basic sky background
    const vec3 gradientStart = vec3(0.5, 0.6, 1.0);
    const vec3 gradientEnd = vec3(1.0);
    vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (unitDir.y + 1.0);
    payload.diffuse = (1.0-t) * gradientStart + t * gradientEnd;

    payload.distance = -1.0f;
    payload.normal = vec3(0.0f);
}