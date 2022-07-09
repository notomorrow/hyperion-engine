#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;
layout(location=1) out vec4 output_normals;
layout(location=2) out vec4 output_positions;

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 18) uniform texture2D ssr_sample;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 21) uniform texture2D ssr_blur_vert;

#include "include/env_probe.inc"
#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/post_fx.inc"
#include "include/tonemap.inc"

#include "include/scene.inc"
#include "include/brdf.inc"

vec2 texcoord = v_texcoord0;//vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);


#define HYP_VCT_ENABLED 1
#define HYP_VCT_REFLECTIONS_ENABLED 1
#define HYP_VCT_INDIRECT_ENABLED 1
#define HYP_ENV_PROBE_ENABLED 1
#define HYP_SSR_ENABLED 1

#if HYP_VCT_ENABLED
#include "include/vct/cone_trace.inc"
#endif

/* Begin main shader program */

#define IBL_INTENSITY 20000.0
#define IRRADIANCE_MULTIPLIER 4.0
#define SSAO_DEBUG 0
#define HYP_CUBEMAP_MIN_ROUGHNESS 0.0

#include "include/rt/probe/shared.inc"

#if 0

vec4 SampleIrradiance(vec3 P, vec3 N, vec3 V)
{
    const uvec3 base_grid_coord    = BaseGridCoord(P);
    const vec3 base_probe_position = GridPositionToWorldPosition(base_grid_coord);
    
    vec3 total_irradiance = vec3(0.0);
    float total_weight    = 0.0;
    
    vec3 alpha = clamp((P - base_probe_position) / PROBE_GRID_STEP, vec3(0.0), vec3(1.0));
    
    for (uint i = 0; i < 8; i++) {
        uvec3 offset = uvec3(i, i >> 1, i >> 2) & uvec3(1);
        uvec3 probe_grid_coord = clamp(base_grid_coord + offset, uvec3(0.0), probe_system.probe_counts.xyz - uvec3(1));
        
        uint probe_index    = GridPositionToProbeIndex(probe_grid_coord);
        vec3 probe_position = GridPositionToWorldPosition(probe_grid_coord);
        vec3 probe_to_point = P - probe_position + (N + 3.0 * V) * PROBE_NORMAL_BIAS;
        vec3 dir            = normalize(-probe_to_point);
        
        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight   = 1.0;
        
        /* Backface test */
        
        vec3 true_direction_to_probe = normalize(probe_position - P);
        weight *= SQR(max(0.0001, (dot(true_direction_to_probe, N) + 1.0) * 0.5)) + 0.2;
        
        /* Visibility test */
        /* TODO */
        
        weight = max(0.000001, weight);
        
        vec3 irradiance_direction = N;
        
    }
}

#endif



void main()
{
    vec4 albedo    = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    vec4 normal    = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);
    vec4 tangent   = vec4(DecodeNormal(SampleGBuffer(gbuffer_tangents_texture, texcoord)), 1.0);
    vec4 bitangent = vec4(DecodeNormal(SampleGBuffer(gbuffer_bitangents_texture, texcoord)), 1.0);
    float depth    = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    vec4 position  = ReconstructPositionFromDepth(inverse(scene.projection * scene.view), texcoord, depth);
    vec4 material  = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */
    
    bool perform_lighting = albedo.a > 0.0;
    
    vec3 albedo_linear = albedo.rgb;
	vec3 result = vec3(0.0);

    /* Physical camera */
    float aperture = 16.0;
    float shutter = 1.0/125.0;
    float sensitivity = 100.0;
    float ev100 = log2((aperture * aperture) / shutter * 100.0f / sensitivity);
    float exposure = 1.0f / (1.2f * pow(2.0f, ev100));
    
    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 L = normalize(scene.light_direction.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);
    vec3 R = normalize(reflect(-V, N));
    vec3 H = normalize(L + V);
    
    float ao = 1.0;
    vec3 irradiance = vec3(0.0);
    vec4 reflections = vec4(0.0);
    vec3 ibl = vec3(0.0);
    
    if (perform_lighting) {
        ao = SampleEffectPre(0, v_texcoord0, vec4(1.0)).r * material.a;

        const float roughness = material.r;
        const float metalness = material.g;
        
        float NdotL = max(0.0001, dot(N, L));
        float NdotV = max(0.0001, dot(N, V));
        float NdotH = max(0.0001, dot(N, H));
        float LdotH = max(0.0001, dot(L, H));
        float VdotH = max(0.0001, dot(V, H));
        float HdotV = max(0.0001, dot(H, V));

        const vec3 diffuse_color = albedo_linear * (1.0 - metalness);

        const float material_reflectance = 0.5;
        const float reflectance = 0.16 * material_reflectance * material_reflectance; // dielectric reflectance
        const vec3 F0 = albedo_linear.rgb * metalness + (reflectance * (1.0 - metalness));
        
        const vec2 AB = BRDFMap(roughness, NdotV);
        const vec3 dfg = albedo_linear.rgb * AB.x + AB.y;
        
        const vec3 energy_compensation = 1.0 + F0 * (AB.y - 1.0);
        const float perceptual_roughness = sqrt(roughness + HYP_CUBEMAP_MIN_ROUGHNESS);
        const float lod = 7.0 * perceptual_roughness * (2.0 - perceptual_roughness);

#if HYP_ENV_PROBE_ENABLED
        if (scene.environment_texture_usage != 0) {
            uint probe_index = scene.environment_texture_index;
            ibl = SampleProbeParallaxCorrected(gbuffer_sampler, env_probe_textures[probe_index], env_probes[probe_index], position.xyz, R, lod).rgb;   //TextureCubeLod(gbuffer_sampler, rendered_cubemaps[scene.environment_texture_index], R, lod).rgb;
        }
#endif

#if HYP_VCT_ENABLED
        vec4 vct_specular = ConeTraceSpecular(position.xyz, N, R, roughness);
        vec4 vct_diffuse  = ConeTraceDiffuse(position.xyz, N, T, B, roughness);

#if HYP_VCT_INDIRECT_ENABLED
        irradiance  = vct_diffuse.rgb;
#endif

#if HYP_VCT_REFLECTIONS_ENABLED
        reflections = vct_specular;
#endif
#endif

#if HYP_SSR_ENABLED
        vec4 screen_space_reflections = Texture2D(gbuffer_sampler, ssr_blur_vert, texcoord);
        screen_space_reflections.rgb = pow(screen_space_reflections.rgb, vec3(2.2));
        reflections = mix(reflections, screen_space_reflections, screen_space_reflections.a);
#endif

        vec3 light_color = vec3(1.0);
        
        // ibl
        //vec3 dfg =  AB;// mix(vec3(AB.x), vec3(AB.y), vec3(1.0) - F0);
        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        vec3 radiance = dfg * ibl * specular_ao;
        result = (diffuse_color * (irradiance * IRRADIANCE_MULTIPLIER) * (1.0 - dfg) * ao) + radiance;
        result *= exposure * IBL_INTENSITY;
        result = (result * (1.0 - reflections.a)) + (dfg * reflections.rgb);
        //end ibl
    } else {
        result = albedo.rgb;
    }

#if SSAO_DEBUG
    result = vec3(ao);
#endif

    output_color = vec4(result, 1.0);

    // output_color.rgb = irradiance.rgb;
    //output_color = ScreenSpaceReflection(material.r);

    //output_color.rgb; = ibl.rgb;
}
