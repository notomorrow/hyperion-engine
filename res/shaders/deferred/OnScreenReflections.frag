#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord;

layout(location=0) out vec4 color_output;

#include "../include/shared.inc"
#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"
#include "../include/tonemap.inc"
#include "../include/noise.inc"

#define HYP_DEFERRED_NO_RT_RADIANCE
#define HYP_DEFERRED_NO_SSR
#define HYP_DEFERRED_NO_ENV_GRID

#include "../include/env_probe.inc"
#include "./DeferredLighting.glsl"
#include "../include/BlueNoise.glsl"

layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 3) readonly buffer EnvProbeBuffer
{
    EnvProbe current_env_probe;
};

layout(push_constant) uniform PushConstant
{
	uvec4 dimensions;
	float ray_step;
	float num_iterations;
	float max_ray_distance;
	float distance_bias;
	float offset;
	float eye_fade_start;
	float eye_fade_end;
	float screen_edge_fade_start;
	float screen_edge_fade_end;
} push_constants;

#define SAMPLE_COUNT 1

mat4 inverse_proj = inverse(camera.projection);
mat4 inverse_view = inverse(camera.view);

bool TraceRays(
    vec3 ray_origin,
    vec3 ray_direction,
    out vec2 hit_pixel,
    out vec3 hit_point,
    out float hit_weight,
    out float num_iterations
)
{
    bool intersect = false;
    num_iterations = 0.0;
    hit_weight = 0.0;
    hit_pixel = vec2(0.0);
    hit_point = vec3(0.0);

    vec3 ray_step = push_constants.ray_step * normalize(ray_direction);
    vec3 marching_position = ray_origin;
    float depth_from_screen;
    float step_delta;

    const mat4 inverse_proj = inverse_proj;

    vec4 view_space_position;

    int i = 0;

    for (; i < push_constants.num_iterations; i++) {
        marching_position += ray_step;

        hit_pixel = GetProjectedPositionFromView(camera.projection, marching_position);
        view_space_position = ReconstructViewSpacePositionFromDepth(inverse_proj, hit_pixel, SampleDepth(hit_pixel));

        step_delta = marching_position.z - view_space_position.z;

        intersect = step_delta > 0.0;
        num_iterations += 1.0;

        if (intersect) {
            break;
        }
    }

    if (intersect) {
        // binary search
        for (; i < push_constants.num_iterations; i++) {
            ray_step *= 0.5;
            marching_position = marching_position - ray_step * sign(step_delta);

            hit_pixel = GetProjectedPositionFromView(camera.projection, marching_position);
            view_space_position = ReconstructViewSpacePositionFromDepth(inverse_proj, hit_pixel, SampleDepth(hit_pixel));
            
            step_delta = abs(marching_position.z) - view_space_position.z;

            if (abs(step_delta) < push_constants.distance_bias) {
                return true;
            }
        }
    }

    return false;
}

float CalculateAlpha(
    float num_iterations,
    vec2 hit_pixel,
    vec3 hit_point,
    float dist,
    vec3 ray_direction
)
{
    float alpha = 1.0;
    // Fade ray hits that approach the maximum iterations
    alpha *= 1.0 - (num_iterations / push_constants.num_iterations);

    // Fade ray hits that approach the screen edge
    vec2 hit_pixel_ndc = hit_pixel * 2.0 - 1.0;
    float max_dimension = saturate(max(abs(hit_pixel_ndc.x), abs(hit_pixel_ndc.y)));
    // alpha *= 1.0 - max(0.0, max_dimension - screen_edge_fade_start) / (1.0 - screen_edge_fade_end);

    alpha = 1.0 - smoothstep(0.06, 0.95, max_dimension);

    return alpha;
}

void SSRGetUV(
    vec3 P, vec3 N, vec3 V, vec3 R, float depth, float roughness, float perceptual_roughness,
    out vec2 hit_pixel, out float alpha
)
{
    vec3 view_space_normal = normalize((camera.view * vec4(N, 0.0)).xyz);
    vec3 ray_origin = P + view_space_normal * 0.01;

    vec3 hit_point;
    float hit_weight;
    float num_iterations;

    bool intersect = TraceRays(ray_origin, R, hit_pixel, hit_point, hit_weight, num_iterations);

    float dist = distance(ray_origin, hit_point);
    alpha = CalculateAlpha(num_iterations, hit_pixel, hit_point, dist, R) * float(intersect);

    hit_pixel *= float(alpha > HYP_FMATH_EPSILON);
    alpha *= float(hit_pixel == saturate(hit_pixel));
}

float IsoscelesTriangleOpposite(float adjacent_length, float cone_theta)
{
    return 2.0 * tan(cone_theta) * adjacent_length;
}

float IsoscelesTriangleInRadius(float a, float h)
{
    float a2 = a * a;
    float fh2 = 4.0 * h * h;

    return (a * (sqrt(a2 + fh2) - a)) / (4.0 * h);
}

float IsoscelesTriangleNextAdjacent(float adjacent_length, float incircle_radius)
{
    // subtract the diameter of the incircle to get the adjacent side of the next level on the cone
    return adjacent_length - (incircle_radius * 2.0);
}

// #define CONE_TRACING

void main()
{
    vec3 irradiance = vec3(0.0);

    const float depth = SampleGBuffer(gbuffer_depth_texture, v_texcoord).r;

    if (depth > 0.9999) {
        color_output = vec4(0.0);

        return;
    }

    const vec3 N = normalize(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, v_texcoord)));
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, v_texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);
    const vec3 R = normalize(reflect(-V, N));

    const vec4 material = SampleGBuffer(gbuffer_material_texture, v_texcoord); 
    const float roughness = material.r;
    const float perceptual_roughness = sqrt(roughness);

    const float lod = float(12.0) * perceptual_roughness * (2.0 - perceptual_roughness);
    
    uvec2 pixel_coord = uvec2(v_texcoord * vec2(push_constants.dimensions.xy) - 0.5);

    vec3 tangent;
    vec3 bitangent;
    ComputeOrthonormalBasis(N, tangent, bitangent);

    // sample from blue noise buffer
    vec2 blue_noise_sample = vec2(
        SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 0),
        SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 1)
    );

    vec2 blue_noise_scaled = blue_noise_sample + float(scene.frame_counter % 256) * 1.618;
    const vec2 rnd = fmod(blue_noise_scaled, vec2(1.0));
    
    // vec3 H = ImportanceSampleGGX(rnd, N, perceptual_roughness);
    // H = tangent * H.x + bitangent * H.y + N * H.z;
    // vec3 dir = normalize(2.0 * dot(V, H) * H - V);

    vec3 H = ImportanceSampleGGX(rnd, N, perceptual_roughness);
    H = tangent * H.x + bitangent * H.y + N * H.z;

    vec3 ssr_ray_dir = reflect(-V, normalize((camera.view * vec4(H, 0.0)).xyz));

    float hit_mask = 0.0;
    vec2 hit_uv = vec2(0.0);

    const vec2 ssr_image_dimensions = vec2(push_constants.dimensions.xy);

    const vec3 view_space_position = (camera.view * vec4(P, 1.0)).xyz;

    SSRGetUV(view_space_position, N, V, ssr_ray_dir, depth, roughness, perceptual_roughness, hit_uv, hit_mask);

    if (hit_mask > 0.0) {
        // Do SSR cone tracing
        vec4 reflection_sample = vec4(0.0);
        float accum_radius = 0.0;

        const float gloss = 1.0 - perceptual_roughness;
        const float cone_angle = RoughnessToConeAngle(perceptual_roughness) * 0.5;

        const float trace_size = float(max(ssr_image_dimensions.x, ssr_image_dimensions.y));
        const float max_mip_level = 9.0;

        vec2 sample_texcoord = v_texcoord;
        const vec2 delta_p = (hit_uv - v_texcoord);

        float adjacent_length = length(delta_p);
        vec2 adjacent_unit = normalize(delta_p);

        float remaining_alpha = 1.0;
        float gloss_multiplier = gloss;

        vec4 accum_color = vec4(0.0);

#ifdef CONE_TRACING
        for (int i = 0; i < 14; i++) {
            const float opposite_length = IsoscelesTriangleOpposite(adjacent_length, cone_angle);
            const float incircle_size = IsoscelesTriangleInRadius(opposite_length, adjacent_length);
            const vec2 sample_position = v_texcoord + adjacent_unit * (adjacent_length - incircle_size);

            const float mip_level = clamp(log2(incircle_size * trace_size), 0.0, max_mip_level);
#else
            const float current_radius = length((hit_uv - v_texcoord) * ssr_image_dimensions) * tan(cone_angle);
            const float mip_level = clamp(log2(current_radius), 0.0, max_mip_level);
#endif

            vec4 current_reflection_sample = Texture2DLod(sampler_linear, gbuffer_mip_chain, hit_uv, mip_level);

#ifdef CONE_TRACING
            current_reflection_sample.rgb *= vec3(gloss_multiplier);
            current_reflection_sample.a = gloss_multiplier;

            remaining_alpha -= current_reflection_sample.a;

            if (remaining_alpha < 0.0) {
                current_reflection_sample.rgb *= (1.0 - abs(remaining_alpha));
            }

            accum_color += current_reflection_sample;

            if (accum_color.a >= 1.0) {
                break;
            }

            adjacent_length = IsoscelesTriangleNextAdjacent(adjacent_length, incircle_size);
            gloss_multiplier *= gloss;
            accum_radius += incircle_size;
        }

        accum_radius /= 14.0;
#else
        accum_color = current_reflection_sample;
#endif

        reflection_sample = accum_color;
        reflection_sample.a *= hit_mask;

        color_output = reflection_sample;

    } else {
        color_output = vec4(0.0);
    }/*else {
        uint _seed = InitRandomSeed(InitRandomSeed(pixel_coord.x, pixel_coord.y), scene.frame_counter % 256);
        vec4 ibl = vec4(0.0);

        const uint probe_texture_index = max(0, min(current_env_probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));

        // // // Do environment probe sample
        for (int i = 0; i < SAMPLE_COUNT; i++) {
            // vec2 rnd = vec2(RandomFloat(_seed), RandomFloat(_seed));

            // vec2 blue_noise_sample = vec2(
            //     SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), i, i * 2),
            //     SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), i, i * 2 + 1)
            // );

            // vec2 blue_noise_scaled = blue_noise_sample + float(scene.frame_counter % 256) * 1.618;
            // const vec2 rnd = fmod(blue_noise_scaled, vec2(1.0));

            vec2 rnd = Hammersley(uint(i), uint(SAMPLE_COUNT));

            H = ImportanceSampleGGX(rnd, N, perceptual_roughness);
            H = tangent * H.x + bitangent * H.y + N * H.z;

            vec3 dir = normalize(2.0 * dot(V, H) * H - V);

            vec4 sample_ibl = vec4(0.0);
            ApplyReflectionProbe(current_env_probe, P, dir, lod, sample_ibl);
            ibl += sample_ibl * (1.0 / float(SAMPLE_COUNT));
        }

        // const vec3 sample_dir = reflect(-V, H);  //normalize(2.0 * dot(V, H) * H - V);;
        // vec4 sample_ibl = vec4(0.0);

        // vec3 dir = sample_dir;
        // ApplyReflectionProbe(current_env_probe, P, dir, lod, sample_ibl);
        // ibl += sample_ibl * (1.0 / 5.0);

        // dir = normalize(sample_dir + tangent);
        // ApplyReflectionProbe(current_env_probe, P, dir, lod, sample_ibl);
        // ibl += sample_ibl * (1.0 / 5.0) * 0.707;

        // dir = normalize(sample_dir + bitangent);
        // ApplyReflectionProbe(current_env_probe, P, dir, lod, sample_ibl);
        // ibl += sample_ibl * (1.0 / 5.0) * 0.707;

        // dir = normalize(sample_dir - tangent);
        // ApplyReflectionProbe(current_env_probe, P, dir, lod, sample_ibl);
        // ibl += sample_ibl * (1.0 / 5.0) * 0.707;

        // dir = normalize(sample_dir - bitangent);
        // ApplyReflectionProbe(current_env_probe, P, dir, lod, sample_ibl);
        // ibl += sample_ibl * (1.0 / 5.0) * 0.707;

        // Do environment probe sample
        // vec3 dir = normalize(2.0 * dot(V, H) * H - V);
        // ApplyReflectionProbe(current_env_probe, P, dir, lod, ibl);

        color_output = ibl;
    }*/



    // // vec4 ibl = CalculateReflectionProbe(current_env_probe, P, N, R, camera.position.xyz, perceptual_roughness);

    // color_output = ibl;
}