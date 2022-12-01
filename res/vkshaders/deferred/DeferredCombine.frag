#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

// layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;


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
// #undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 55) uniform texture2D deferred_indirect_lighting;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 56) uniform texture2D deferred_direct_lighting;

// layout(set = 0, binding = 0) uniform texture2D deferred_indirect_lighting;
// layout(set = 0, binding = 1) uniform texture2D deferred_direct_lighting;
// layout(set = 0, binding = 2) uniform texture2D gbuffer_mip_chain;
// layout(set = 0, binding = 3) uniform sampler HYP_SAMPLER_NEAREST;
// layout(set = 0, binding = 4) uniform sampler HYP_SAMPLER_LINEAR;
// layout(set = 0, binding = 5, rgba16f) uniform image2D combined_result;

// layout(std140, set = 0, binding = 6, row_major) readonly buffer SceneShaderData
// {
//     Scene scene;
// };

// layout(set = 0, binding = 7) uniform texture2D gbuffer_textures[NUM_GBUFFER_TEXTURES];
// layout(set = 0, binding = 8) uniform texture2D gbuffer_depth_texture;

#define HYP_DEFERRED_NO_RT_RADIANCE // temp
#define HYP_DEFERRED_NO_SSR // temp
#define HYP_DEFERRED_NO_ENV_PROBE // temp
#include "./DeferredLighting.glsl"

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
    // const uvec2 index = gl_GlobalInvocationID.xy;

    // if (any(greaterThanEqual(index, image_dimensions))) {
    //     return;
    // }

    // const vec2 texcoord = vec2(index) / vec2(image_dimensions - 1);

    vec4 indirect_lighting = Texture2D(HYP_SAMPLER_NEAREST, deferred_indirect_lighting, texcoord);
    indirect_lighting.a = 1.0;

    vec4 direct_lighting = Texture2D(HYP_SAMPLER_NEAREST, deferred_direct_lighting, texcoord);

    vec4 forward_result = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_albedo_texture_translucent, texcoord);

    vec4 material = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_material_texture, texcoord);
    const float roughness = material.r;
    const float metalness = material.g;
    const float transmission = material.b;
    const float ao = material.a;
    const float perceptual_roughness = sqrt(roughness);

    const vec3 N = DecodeNormal(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_normals_texture, texcoord));
    const float depth = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_depth_texture, texcoord).r;
    const vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(scene.projection), inverse(scene.view), texcoord, depth);
    const vec3 P = position.xyz;
    const vec3 V = normalize(scene.camera_position.xyz - position.xyz);
    const vec3 R = normalize(reflect(-V, N));

    vec4 result = vec4(
        (direct_lighting.rgb * direct_lighting.a) + (indirect_lighting.rgb * (1.0 - direct_lighting.a)),
        1.0
    );

    if (transmission > 0.0) {
        const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));
        const vec3 F0 = CalculateF0(forward_result.rgb, metalness);
        const vec3 F = CalculateFresnelTerm(F0, roughness, NdotV);

        const vec3 dfg = CalculateDFG(F, roughness, NdotV);
        const vec3 E = mix(dfg.xxx, dfg.yyy, F0);

        vec3 Ft = CalculateRefraction(
            image_dimensions,
            P, N, V,
            texcoord,
            F0, E,
            transmission, roughness,
            result, forward_result,
            vec3(ao)
        );

        vec3 Fd = forward_result.rgb * (1.0 /*irradiance*/) * (1.0 - E) * (1.0 * ao/* diffuse BRDF */);
        Fd *= (1.0 - transmission);


        vec3 ibl = vec3(0.0);

// #ifdef ENV_PROBE_ENABLED
//         ibl = CalculateEnvProbeReflection(deferred_params, position.xyz, N, R, perceptual_roughness);
// #endif

        vec3 Fr = E * ibl;
        

        // TODO: Specular highlight, irradiance...

        result.rgb = Ft + Fd + Fr;

    } else {
        result.rgb = (forward_result.rgb * forward_result.a) + (result.rgb * (1.0 - forward_result.a));
    }

    result.a = 1.0;
    
    // result = vec4(forward_result.a, 0.0, 0.0, 1.0);

    // TODO: Do post FX combine here instead of in blit.frag
    
    // imageStore(combined_result, ivec2(index), result);

    color_output = result;
}