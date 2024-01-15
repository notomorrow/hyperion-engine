#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 texcoord;
layout(location=0) out vec4 color_output;

// #define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/shared.inc"
#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"
#include "../include/env_probe.inc"
// #undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 45) uniform texture2D rt_radiance_final;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 39) uniform texture2D ssr_result;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 55) uniform texture2D deferred_indirect_lighting;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 56) uniform texture2D deferred_direct_lighting;

#include "./DeferredLighting.glsl"
#include "../include/shadows.inc"

#define HYP_DEFERRED_NO_RT_RADIANCE // temp
// #define HYP_DEFERRED_NO_ENV_PROBE // temp

layout(push_constant) uniform DeferredCombineData
{
    uvec2 image_dimensions;
    uint _pad0;
    uint _pad1;
    DeferredParams deferred_params;
};

#if 0
struct Refraction
{
    vec3 position;
    vec3 direction;
};

float ApplyIORToRoughness(float ior, float roughness)
{
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}

void RefractionSolidSphere(
    vec3 P, vec3 N, vec3 V, float eta_ir,
    out Refraction out_refraction
)
{
    const float thickness = 0.8;

    const vec3 R = refract(-V, N, eta_ir);
    float NdotR = dot(N, R);
    float d = thickness * -NdotR;

    Refraction refraction;
    refraction.position = vec3(P + R * d);
    vec3 n1 = normalize(NdotR * R - N * 0.5);
    refraction.direction = refract(R, n1, eta_ir);

    out_refraction = refraction;
}
#endif

void main()
{
    vec4 lit_result = Texture2D(HYP_SAMPLER_NEAREST, deferred_indirect_lighting, texcoord);
    lit_result.a = 1.0;

    vec4 forward_result = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_albedo_texture_translucent, texcoord);
    vec3 albedo = forward_result.rgb;

    vec4 material = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_material_texture, texcoord);
    const float roughness = material.r;
    const float metalness = material.g;
    const float transmission = material.b;
    const float ao = material.a;
    const float perceptual_roughness = sqrt(roughness);

    const mat4 inverse_proj = inverse(camera.projection);
    const mat4 inverse_view = inverse(camera.view);

    const vec3 N = DecodeNormal(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_normals_texture, texcoord));
    const float depth = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_depth_texture, texcoord).r;
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);
    const vec3 R = normalize(reflect(-V, N));

    const uint object_mask = VEC4_TO_UINT(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_mask_texture, texcoord));

    vec3 L = light.position_intensity.xyz;
    L -= P * float(min(light.type, 1));
    L = normalize(L);

    const vec3 H = normalize(L + V);
    const float NdotL = max(0.0001, dot(N, L));
    const float NdotH = max(0.0001, dot(N, H));
    const float LdotH = max(0.0001, dot(L, H));
    const float HdotV = max(0.0001, dot(H, V));

    // vec4 result = vec4(
    //     (direct_lighting.rgb * direct_lighting.a) + (indirect_lighting.rgb * (1.0 - direct_lighting.a)),
    //     1.0
    // );

    vec4 result = lit_result;

    if (bool(object_mask & (0x02 | 0x400)) && !bool(object_mask & 0x10)) {
        result.rgb = (forward_result.rgb * forward_result.a) + (result.rgb * (1.0 - forward_result.a));
    } else {
        result.rgb += forward_result.rgb * forward_result.a;
    }

    // result.rgb = forward_result.aaa;

    // result.rgb = (forward_result.rgb * forward_result.a) + (result.rgb * (1.0 - forward_result.a));

    // result.rgb = forward_result.aaa;
    // result.rgb = forward_lit_result.rgb;
    // result.rgb = vec3(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_mask_texture, texcoord).rgb);//  DecodeNormal(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_normals_texture, texcoord));
    result.a = 1.0;

    color_output = result;
}