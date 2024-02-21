#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "../include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=4) in vec3 v_tangent;
layout(location=5) in vec3 v_bitangent;
layout(location=7) in flat vec3 v_camera_position;
layout(location=8) in mat3 v_tbn_matrix;
layout(location=11) in flat uint v_object_index;
layout(location=12) in flat vec3 v_env_probe_extent;
layout(location=13) in flat uint v_cube_face_index;
layout(location=14) in vec2 v_cube_face_uv;
layout(location=15) in vec4 v_view_space_position;

layout(location=0) out vec4 output_color;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/scene.inc"
#include "../include/shared.inc"
#include "../include/material.inc"
#include "../include/gbuffer.inc"
#include "../include/env_probe.inc"
#include "../include/tonemap.inc"
#include "../include/noise.inc"
#include "../include/Octahedron.glsl"
#include "../include/object.inc"
#include "../include/packing.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentEnvProbe, size = 512) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, size = 33554432) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, LightsBuffer, size = 64) readonly buffer LightsBuffer
{
    Light light;
};

// #ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Object, MaterialsBuffer, size = 8388608) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef CURRENT_MATERIAL
    #define CURRENT_MATERIAL (materials[object.material_index])
#endif
// #else

// HYP_DESCRIPTOR_SSBO_DYNAMIC(Object, MaterialsBuffer, size = 128) readonly buffer MaterialsBuffer
// {
//     Material material;
// };

// #ifndef CURRENT_MATERIAL
//     #define CURRENT_MATERIAL material
// #endif
// #endif

#define SUN_INTENSITY 12.0

#define PLANET_RADIUS 6371e3
#define ATMOSPHERE_RADIUS 6471e3

#define RAYLEIGH_SCATTER_COEFF vec3(5.5e-6, 13.0e-6, 22.4e-6)
#define RAYLEIGH_SCATTER_HEIGHT 8e3

#define MIE_SCATTER_COEFF 21e-6
#define MIE_SCATTER_HEIGHT 1.2e3
#define MIE_SCATTER_DIRECTION 0.758

#define NUM_STEPS_X 16
#define NUM_STEPS_Y 16

vec2 RaySphereIntersection(vec3 r0, vec3 rd, float sr)
{
    float a = dot(rd, rd);
    float b = 2.0 * dot(rd, r0);
    float c = dot(r0, r0) - (sr * sr);
    float d = (b * b) - 4.0 * a * c;

    if (d < 0.0) {
        return vec2(1e5, -1e5);
    }

    return vec2(
        (-b - sqrt(d)) / (2.0 * a),
        (-b + sqrt(d)) / (2.0 * a)
    );
}

vec3 GetAtmosphere(vec3 ray_direction, vec3 light_direction)
{
    const vec3 ray_origin = vec3(0.0, 6372e3, 0.0);

    vec2 p = RaySphereIntersection(ray_origin, ray_direction, ATMOSPHERE_RADIUS);
    if (p.x > p.y) {
        return vec3(0.0);
    }

    p.y = min(p.y, RaySphereIntersection(ray_origin, ray_direction, PLANET_RADIUS).x);

    vec2 step_size = vec2((p.y - p.x) / float(NUM_STEPS_X), 0.0);

    // Initialize accumulators for Rayleigh and Mie scattering.
    vec3 total_rayleigh = vec3(0.0);
    vec3 total_mie = vec3(0.0);

    // Initialize optical depth accumulators for the primary ray.
    vec2 rayleigh_depths = vec2(0.0);
    vec2 mie_depths = vec2(0.0);

    // Calculate the Rayleigh and Mie phases.
    const float mu = dot(ray_direction, light_direction);
    const float mumu = mu * mu;
    const float g = MIE_SCATTER_DIRECTION;
    const float gg = g * g;
    float pRlh = 3.0 / (16.0 * HYP_FMATH_PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * HYP_FMATH_PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));

    float time = 0.0;

    // Sample the primary ray.
    for (int i = 0; i < NUM_STEPS_X; i++) {
        // Calculate the primary ray sample position.
        vec3 pos_x = ray_origin + ray_direction * (time + step_size.x * 0.5);

        vec2 heights = vec2(0.0);

        // Calculate the height of the sample.
        heights.x = length(pos_x) - PLANET_RADIUS;

        // Calculate the optical depth of the Rayleigh and Mie scattering for this step.
        float rayleigh_depth = exp(-heights.x / RAYLEIGH_SCATTER_HEIGHT) * step_size.x;
        float mie_depth = exp(-heights.x / MIE_SCATTER_HEIGHT) * step_size.x;

        // Accumulate optical depth.
        rayleigh_depths.x += rayleigh_depth;
        mie_depths.x += mie_depth;

        // Calculate the step size of the secondary ray.
        step_size.y = RaySphereIntersection(pos_x, light_direction, ATMOSPHERE_RADIUS).y / float(NUM_STEPS_Y);

        // Initialize the secondary ray time.
        float ray_time = 0.0;

        // Initialize optical depth accumulators for the secondary ray.
        rayleigh_depths.y = 0.0;
        mie_depths.y = 0.0;

        // Sample the secondary ray.
        for (int j = 0; j < NUM_STEPS_Y; j++) {

            // Calculate the secondary ray sample position.
            vec3 pos_y = pos_x + light_direction * (ray_time + step_size.y * 0.5);

            // Calculate the height of the sample.
            heights.y = length(pos_y) - PLANET_RADIUS;

            // Accumulate the optical depth.
            rayleigh_depths.y += exp(-heights.y / RAYLEIGH_SCATTER_HEIGHT) * step_size.y;
            mie_depths.y += exp(-heights.y / MIE_SCATTER_HEIGHT) * step_size.y;

            // Increment the secondary ray time.
            ray_time += step_size.y;
        }

        // Calculate attenuation.
        vec3 attenuation = exp(-(MIE_SCATTER_COEFF * (mie_depths.x + mie_depths.y) + RAYLEIGH_SCATTER_COEFF * (rayleigh_depths.x + rayleigh_depths.y)));

        // Accumulate scattering.
        total_rayleigh += rayleigh_depth * attenuation;
        total_mie += mie_depth * attenuation;

        // Increment the primary ray time.
        time += step_size.x;
    }

    // Calculate and return the final color.
    return SUN_INTENSITY * (pRlh * RAYLEIGH_SCATTER_COEFF * total_rayleigh + pMie * MIE_SCATTER_COEFF * total_mie);
}

void main()
{
    vec3 normal = normalize(v_normal);

    vec4 sky_color = vec4(0.0);
    
#ifdef CUTOFF
    const vec3 sky_color_bottom = vec3(0.0);

    sky_color = sky_color_bottom;

    if (v_position.y >= CUTOFF) {
#endif
        float dist = length(v_position);

        vec3 light_direction = normalize(light.position_intensity.xyz);
        vec3 ray_direction = normalize(v_position);

        vec3 atmosphere = GetAtmosphere(ray_direction, light_direction);

        sky_color = vec4(atmosphere, 1.0);
#ifdef CUTOFF
    }

    sky_color = mix(sky_color, sky_color_bottom, 1.0 - smoothstep(CUTOFF, 0.0, v_position.y));
#endif

    // exposure
    sky_color.rgb = 1.0 - exp(-1.0 * sky_color.rgb);;

    // // sky will not be tonemapped outside of this:
    // sky_color = vec4(1.0, 0.0, 0.0, 1.0);//TonemapReinhardSimple(sky_color.rgb);

    output_color = sky_color;
}