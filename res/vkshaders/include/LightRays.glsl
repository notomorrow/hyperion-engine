#ifndef LIGHT_RAYS_GLSL
#define LIGHT_RAYS_GLSL

#include "shadows.inc"

#define VOLUMETRIC_LIGHTING_STEPS 64
#define VOLUMETRIC_LIGHTING_MIE_G 0.995
#define VOLUMETRIC_LIGHTING_FADE_START 0.95
#define VOLUMETRIC_LIGHTING_FADE_END 0.9
#define VOLUMETRIC_LIGHTING_INTENSITY 0.9

float LightRayAttenuation(uint shadow_map_index, vec3 P, float NdotL)
{
    // Get shadow
    return GetShadowStandard(shadow_map_index, P, vec2(0.0), NdotL);
}

float LightRayDensity(vec3 position)
{
    // TODO: noise
    return 1.0;
}

float LightRayScattering(float f_cos, float f_cos2, float g, float g2)
{
    return 1.5 * ((1.0 - g2) / (2.0 + g2)) * (1.0 + f_cos2) / pow(1.0 + g2 - 2.0 * g * f_cos, 1.5);
}

float LightRayMarch(uint shadow_map_index, vec2 uv, vec3 L, vec3 ray_origin, vec3 ray_dir, float ray_len)
{
    float step_size = ray_len / float(VOLUMETRIC_LIGHTING_STEPS);
    vec3 ray_delta_step = ray_dir * step_size;

    const float extinction_value = 0.0025;
    const float scattering_value = 0.75;

    float volumetric = 0.0;
    float extinction = 0.0;
    
    vec3 marching_position = ray_origin + ray_delta_step;
    float cos_angle = dot(L, ray_dir);

    for (int i = 0; i < VOLUMETRIC_LIGHTING_STEPS; i++) {
        float attenuation = LightRayAttenuation(shadow_map_index, marching_position, cos_angle);

        // float density = LightRayDensity(marching_position);
        // float scattering = scattering_value * step_size * density;
        // extinction += extinction_value * step_size * density;
        
        volumetric += attenuation;//* scattering * density;// * exp(-extinction);
        marching_position += ray_delta_step;
    }

    volumetric /= float(VOLUMETRIC_LIGHTING_STEPS);
    volumetric *= saturate(LightRayScattering(cos_angle, cos_angle * cos_angle, VOLUMETRIC_LIGHTING_MIE_G, VOLUMETRIC_LIGHTING_MIE_G * VOLUMETRIC_LIGHTING_MIE_G));
    volumetric = max(0.0, volumetric);
    // volumetric *= exp(-extinction);

    return volumetric;
}

float LightRays(uint shadow_map_index, vec2 uv, vec3 L, vec3 P, vec3 camera_position, float linear_depth)
{
    vec3 ray_dir = P - camera_position;
    float ray_len = length(ray_dir);
    ray_dir /= ray_len;
    ray_len = min(ray_len, linear_depth);

    return LightRayMarch(shadow_map_index, uv, L, camera_position, ray_dir, ray_len);
}

#endif