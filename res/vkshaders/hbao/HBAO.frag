#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 texcoord;
layout(location=0) out vec4 color_output;

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/noise.inc"
#include "../include/packing.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(set = 0, binding = 0) uniform texture2D gbuffer_albedo_texture;
layout(set = 0, binding = 1) uniform texture2D gbuffer_normals_texture;
layout(set = 0, binding = 2) uniform texture2D gbuffer_depth_texture;
layout(set = 0, binding = 3) uniform texture2D gbuffer_velocity_texture;
layout(set = 0, binding = 4) uniform texture2D gbuffer_material_texture;
layout(set = 0, binding = 5) uniform sampler sampler_nearest;
layout(set = 0, binding = 6) uniform sampler sampler_linear;

layout(std140, set = 0, binding = 7, row_major) readonly buffer SceneBuffer
{
    Scene scene;
};

layout(push_constant) uniform PushConstant
{
    uvec2 dimension;
    float temporal_blending_factor;
};

#include "../include/Temporal.glsl"


#define HYP_HBAO_NUM_CIRCLES 4
#define HYP_HBAO_NUM_SLICES 2
#define HYP_HBAO_RADIUS 32.0
#define HYP_HBAO_POWER 1.15

float fov_rad = HYP_FMATH_DEG2RAD(scene.camera_fov);
float tan_half_fov = tan(fov_rad * 0.5);
float inv_tan_half_fov = 1.0 / tan_half_fov;
vec2 focal_len = vec2(inv_tan_half_fov * (float(scene.resolution_y) / float(scene.resolution_x)), inv_tan_half_fov);
vec2 inv_focal_len = vec2(1.0) / focal_len;
vec4 uv_to_view = vec4(2.0 * inv_focal_len.x, 2.0 * inv_focal_len.y, -1.0 * inv_focal_len.x, -1.0 * inv_focal_len.y);

float GetOffsets(vec2 uv)
{
    ivec2 position = ivec2(uv * vec2(dimension));
    return 0.25 * float((position.y - position.x) & 3);
}

mat4 inv_view_proj = inverse(scene.projection * scene.view);
mat4 inv_view = inverse(scene.view);
mat4 inv_proj = inverse(scene.projection);

float GetDepth(vec2 uv)
{
    return Texture2DLod(sampler_nearest, gbuffer_depth_texture, uv, 0.0).r;
}

vec3 GetPosition(vec2 uv, float depth)
{
    return ReconstructViewSpacePositionFromDepth(inv_proj, uv, depth).xyz;
    // const float linear_depth = LinearDepth(scene.projection, depth);
    // const float linear_depth = ViewDepth(depth, scene.camera_near, scene.camera_far);
    // return vec3((vec2(uv.x, 1.0 - uv.y) * uv_to_view.xy + uv_to_view.zw) * linear_depth, linear_depth);

    vec4 v = inv_proj * vec4(uv * 2.0 - 1.0, depth, 1.0);
    v /= v.w;
    return v.xyz;
    

    // vec4 pos_world = ReconstructWorldSpacePositionFromDepth(inv_proj, inv_view, uv, depth);
    // vec4 pos_view = scene.view * pos_world;
    // pos_view /= pos_view.w;
    // return pos_view.xyz;
}

vec3 GetNormal(vec2 uv)
{
    vec3 normal = DecodeNormal(Texture2DLod(sampler_nearest, gbuffer_normals_texture, uv, 0.0));
    vec3 view_normal = (scene.view * vec4(normal, 0.0)).xyz;

    return view_normal;
}

float IntegrateUniformWeight(vec2 h)
{
    vec2 arc = vec2(1.0) - cos(h);

    return arc.x + arc.y;
}

float IntegrateArcCosWeight(vec2 h, float n)
{
    vec2 arc = -cos(2.0 * h - n) + cos(n) + 2.0 * h * sin(n);

    return 0.25 * (arc.x + arc.y);
}

vec2 RotateDirection(vec2 uv, vec2 cos_sin)
{
    return vec2(
        uv.x * cos_sin.x - uv.y * cos_sin.y,
        uv.x * cos_sin.y + uv.y * cos_sin.x
    );
}

#define ANGLE_BIAS 0.1

// vec3 GetCameraSpaceRay(in vec3 V, in vec2 ray)
// {
//     return normalize(vec3(ray * -V.z, dot(V.xy, ray)));
// }
float Falloff(float dist_sqr)
{
    return dist_sqr * (-1.0 / HYP_FMATH_SQR(HYP_HBAO_RADIUS)) + 1.0;
}

float CalcAO(vec3 P, vec3 N, vec2 uv)
{
    vec3 S = GetPosition(uv, GetDepth(uv));

    vec3 V = S - P;
    float VdotV = dot(V, V);
    float NdotV = dot(N, V) * inversesqrt(VdotV);

    return Saturate(NdotV - ANGLE_BIAS) * Saturate(Falloff(VdotV));
}

void TraceAO_New(vec2 uv, out float occlusion, out vec3 bent_normal, out vec4 light_color)
{
    uvec2 pixel_coord = clamp(uvec2(uv * vec2(dimension) - 0.5), uvec2(0), dimension - 1);
    uint seed = InitRandomSeed(InitRandomSeed(pixel_coord.x, pixel_coord.y), scene.frame_counter % 6);

    const float projected_scale = float(dimension.y) / (tan_half_fov * 2.0);

    const float temporal_offset = GetSpatialOffset(scene.frame_counter);
    const float temporal_rotation = GetTemporalRotation(scene.frame_counter);

    const float noise_offset = GetOffsets(uv);
    const float noise_direction = RandomFloat(seed);//InterleavedGradientNoise(vec2(pixel_coord));
    const float ray_step = fract(noise_offset + temporal_offset);

    const float depth = GetDepth(uv);
    const vec3 P = GetPosition(uv, depth);
    const vec3 N = GetNormal(uv);
    const vec3 V = normalize(vec3(0.0) - P);

    const float camera_distance = P.z;
    const vec2 texel_size = vec2(1.0) / vec2(dimension);
    const float step_radius = max((projected_scale * HYP_HBAO_RADIUS) / max(camera_distance, HYP_FMATH_EPSILON), float(HYP_HBAO_NUM_SLICES)) / float(HYP_HBAO_NUM_SLICES + 1);

    occlusion = 0.0;
    bent_normal = vec3(0.0);
    light_color = vec4(0.0);

#if 0
    float small_scale_ao = 0.0;
    float large_scale_ao = 0.0;

    vec3 rand = vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed));

    const float alpha = 2.0 * HYP_FMATH_PI / HYP_HBAO_NUM_CIRCLES;
    for (int i = 0; i < HYP_HBAO_NUM_CIRCLES; i++) {
        float angle = float(i) * alpha;

        vec2 direction = RotateDirection(vec2(cos(angle), sin(angle)), rand.xy);
        float ray_pixels = (rand.z * step_radius + 1.0);

        small_scale_ao += CalcAO(P, N, uv + (direction * ray_pixels));
        ray_pixels += step_radius;

        for (int j = 1; j < HYP_HBAO_NUM_SLICES; j++) {
            large_scale_ao += CalcAO(P, N, uv + (direction * ray_pixels));
            ray_pixels += step_radius;
        }
    }

    occlusion = (small_scale_ao) + (large_scale_ao);
    occlusion /= float(HYP_HBAO_NUM_CIRCLES * HYP_HBAO_NUM_SLICES);
#endif

#if 1
    for (int i = 0; i < HYP_HBAO_NUM_CIRCLES; i++) {
        float angle = (float(i) + noise_direction + ((temporal_rotation / 360.0))) * (HYP_FMATH_PI / float(HYP_HBAO_NUM_CIRCLES)) * 2.0;

        vec2 ss_ray = vec2(sin(angle), cos(angle));
        const vec4 integral_factors = vec4(vec2(dot(N.xy, ss_ray), N.z), vec2(dot(N.xy, -ss_ray), N.z));

        vec2 horizons = vec2(-1.0);
        vec2 slice_ao = vec2(0.0);
        vec4 slice_light[2] = { vec4(0.0), vec4(0.0) };

        for (int j = 0; j < HYP_HBAO_NUM_SLICES; j++) {
            vec2 uv_offset = (ss_ray * texel_size) * max(step_radius * (float(j) + ray_step), float(j + 1));
            // uv_offset *= mix(-1.0, 1.0, float(j % 2 == 0));

            vec4 new_uv = uv.xyxy + vec4(uv_offset, -uv_offset);

            // Fade hits that approach the screen edge
            const vec4 new_uv_ndc = new_uv * 2.0 - 1.0;
            const float max_dimension = min(1.0, max(abs(new_uv_ndc.x), abs(new_uv_ndc.y)));
            const float fade = 1.0 - saturate(max(0.0, max_dimension - 0.95) / (1.0 - 0.98));

            if (new_uv.xy == saturate(new_uv.xy)) {
                // new_uv = Saturate(new_uv);

                vec3 ds = GetPosition(new_uv.xy, GetDepth(new_uv.xy)) - P;
                vec3 dt = GetPosition(new_uv.zw, GetDepth(new_uv.zw)) - P;

                const vec2 len = vec2(length(ds), length(dt));
                const vec2 dist = len / HYP_HBAO_RADIUS;
                ds /= len.x;
                dt /= len.y;

                const vec2 DdotD = vec2(dot(ds, ds), dot(dt, dt));
                const vec2 NdotD = vec2(dot(ds, N), dot(dt, N));// * (1.0 / sqrt(DdotD));
                // const vec2 VdotD = vec2(dot(ds, V), dot(dt, V));

                const vec2 H = NdotD * inversesqrt(DdotD);

                const vec2 falloff = saturate(DdotD * (2.0 / HYP_FMATH_SQR(HYP_HBAO_RADIUS)));

                const vec2 color_weights = step(0.05, dist) * fade;
                const vec2 condition = vec2(greaterThan(H, horizons)) * vec2(lessThan(dist, vec2(0.9995)));

                vec4 new_color_0 = Texture2DLod(sampler_linear, gbuffer_albedo_texture, new_uv.xy, 0.0);
                const vec4 material_0 = Texture2DLod(sampler_nearest, gbuffer_material_texture, new_uv.xy, 0.0);

                vec4 new_color_1 = Texture2DLod(sampler_linear, gbuffer_albedo_texture, new_uv.zw, 0.0);
                const vec4 material_1 = Texture2DLod(sampler_nearest, gbuffer_material_texture, new_uv.zw, 0.0);

                // metallic objects do not emit diffuse light
                new_color_0 *= 1.0 - material_0.g;
                new_color_1 *= 1.0 - material_1.g;

                vec2 falloffs = saturate(vec2(Falloff(DdotD.x), Falloff(DdotD.y)));

                slice_light[0] += (slice_light[0] + vec4(new_color_0) * (NdotD.x - ANGLE_BIAS) * (falloffs.x)) * condition.x;
                slice_light[1] += (slice_light[1] + vec4(new_color_1) * (NdotD.y - ANGLE_BIAS) * (falloffs.y)) * condition.y;

                // slice_light[0] = mix(
                //     slice_light[0],
                //     vec4(new_color_0) * NdotD.x,
                //     condition.x * fade
                // );
                // slice_light[1] = mix(
                //     slice_light[1],
                //     vec4(new_color_1) * NdotD.y,
                //     condition.y * fade
                // );

                // slice_light[0] = mix(slice_light[0], vec4(NdotD.xxx, 1.0), 1.0);
                // slice_light[1] = mix(slice_light[1], vec4(NdotD.yyy, 1.0), 1.0);

                slice_ao += vec2(
                    ((NdotD.x - ANGLE_BIAS) * falloffs.x),//(1.0 - HYP_FMATH_SQR(dist.x)) * (NdotD.x - slice_ao.x),
                    ((NdotD.y - ANGLE_BIAS) * falloffs.y)//(1.0 - HYP_FMATH_SQR(dist.y)) * (NdotD.y - slice_ao.y)
                ) * condition * fade;

                horizons = mix(horizons, H, condition);
            }
        }

        light_color += slice_light[0] + slice_light[1];
        occlusion += slice_ao.x + slice_ao.y;
    }

    occlusion = 1.0 - Saturate(pow(occlusion / float(HYP_HBAO_NUM_CIRCLES * HYP_HBAO_NUM_SLICES), 1.0 / HYP_HBAO_POWER));
    occlusion *= 1.0 / (1.0 - ANGLE_BIAS);

    light_color = light_color / float(HYP_HBAO_NUM_CIRCLES * HYP_HBAO_NUM_SLICES);
    light_color *= 1.0 / (1.0 - ANGLE_BIAS);

    #endif
}

void main()
{
    float occlusion;
    vec3 bent_normal;
    vec4 light_color;

    TraceAO_New(texcoord, occlusion, bent_normal, light_color);

    vec4 next_values = vec4(light_color.rgb, occlusion);
    next_values.rgb = pow(next_values.rgb, vec3(1.0 / 2.2));

    color_output = next_values;
}