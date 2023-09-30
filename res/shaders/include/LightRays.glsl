#ifndef LIGHT_RAYS_GLSL
#define LIGHT_RAYS_GLSL

#include "shadows.inc"

#define VOLUMETRIC_LIGHTING_STEPS 64
#define VOLUMETRIC_LIGHTING_MIE_G 0.758
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
    // return HYP_FMATH_QUARTER_PI * ((1.0 - g2) / (pow((1.0 + g2) - (2.0 * g) * f_cos, 1.5)));
    return 1.5 * ((1.0 - g2) / (2.0 + g2)) * (1.0 + f_cos2) / pow(1.0 + g2 - 2.0 * g * f_cos, 1.5);
}

float LightRayMarch(uint shadow_map_index, vec2 uv, vec3 L, vec3 ray_origin, vec3 ray_dir, float ray_len)
{
    float step_size = ray_len / float(VOLUMETRIC_LIGHTING_STEPS);
    vec3 ray_delta_step = ray_dir * step_size;

    const float extinction_value = 0.0015;
    const float scattering_value = 1.0;

    float volumetric = 0.0;
    float extinction = 0.0;
    
    float cos_angle = dot(ray_dir, L);

    float r = rand(ray_origin.xy + ray_origin.z);
    vec3 marching_position = ray_origin + (ray_delta_step * (1.0 + r));

    for (int i = 0; i < VOLUMETRIC_LIGHTING_STEPS; i++) {
        float attenuation = LightRayAttenuation(shadow_map_index, marching_position, cos_angle);

        float density = LightRayDensity(marching_position);
        float scattering = scattering_value * step_size * density;
        extinction += extinction_value * step_size * density;
        
        volumetric += attenuation * scattering * density * exp(-extinction);
        marching_position += ray_delta_step;
    }

    volumetric /= float(VOLUMETRIC_LIGHTING_STEPS);
    volumetric *= saturate(LightRayScattering(cos_angle, cos_angle * cos_angle, VOLUMETRIC_LIGHTING_MIE_G, VOLUMETRIC_LIGHTING_MIE_G * VOLUMETRIC_LIGHTING_MIE_G));
    volumetric = max(0.0, volumetric);
    volumetric *= exp(-extinction);

    return volumetric;
}

float LightRays(uint shadow_map_index, vec2 uv, vec3 L, vec3 P, vec3 camera_position, float depth)
{
    float linear_depth = Linear01Depth(depth, camera.near, camera.far);

    vec3 ray_start = camera.position.xyz;

    vec3 ray_dir = P - ray_start;
    ray_dir *= linear_depth;

    float ray_len = length(ray_dir);
    ray_dir /= ray_len;

    ray_len = min(ray_len, 120.0);

    return LightRayMarch(shadow_map_index, uv, L, ray_start, ray_dir, ray_len);
}

float LightRays2(vec2 uv, vec2 ss_L, vec3 P, vec3 camera_position)
{
    const float density = 0.98;
    const float decay = 0.95;

    vec2 delta_uv = uv - ss_L;
    delta_uv *= (1.0 / float(VOLUMETRIC_LIGHTING_STEPS)) * density;
    
    vec2 march_uv = uv;

    float illum_decay = 1.0;

    float attenuation = 0.0;

    for (int i = 0; i < VOLUMETRIC_LIGHTING_STEPS; i++) {
        march_uv -= delta_uv;

        uint ss_object_mask = VEC4_TO_UINT(Texture2D(sampler_linear, gbuffer_mask_texture, march_uv));
        bool occluded = !bool(ss_object_mask & (0x01 | 0x02));
        float occlusion = float(occluded);
        occlusion *= illum_decay;

        attenuation += occlusion * 0.01;

        illum_decay *= decay;
    }

    return attenuation;
}

#endif