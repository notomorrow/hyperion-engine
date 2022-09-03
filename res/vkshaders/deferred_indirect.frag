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
#include "include/PostFXSample.inc"
#include "include/tonemap.inc"

#include "include/scene.inc"
#include "include/brdf.inc"
#include "include/PhysicalCamera.inc"

vec2 texcoord = v_texcoord0;

#define HYP_VCT_ENABLED 1
#define HYP_VCT_REFLECTIONS_ENABLED 1
#define HYP_VCT_INDIRECT_ENABLED 1
#define HYP_ENV_PROBE_ENABLED 1
#define HYP_SSR_ENABLED 1

#if HYP_VCT_ENABLED
#include "include/vct/cone_trace.inc"
#endif

/* Begin main shader program */

#define IBL_INTENSITY 100000.0
#define IRRADIANCE_MULTIPLIER 1.0
#define SSAO_DEBUG 0
#define HYP_CUBEMAP_MIN_ROUGHNESS 0.0

#include "include/rt/probe/shared.inc"

#define DEFERRED_FLAG_SSR_ENABLED 0x1

layout(push_constant) uniform DeferredParams {
    uint flags;
} deferred_params;

#if 0

vec4 SampleIrradiance(vec3 P, vec3 N, vec3 V)
{
    const uvec3 base_grid_coord = BaseGridCoord(P);
    const vec3 base_probe_position = GridPositionToWorldPosition(base_grid_coord);
    
    vec3 total_irradiance = vec3(0.0);
    float total_weight = 0.0;
    
    vec3 alpha = clamp((P - base_probe_position) / PROBE_GRID_STEP, vec3(0.0), vec3(1.0));
    
    for (uint i = 0; i < 8; i++) {
        uvec3 offset = uvec3(i, i >> 1, i >> 2) & uvec3(1);
        uvec3 probe_grid_coord = clamp(base_grid_coord + offset, uvec3(0.0), probe_system.probe_counts.xyz - uvec3(1));
        
        uint probe_index = GridPositionToProbeIndex(probe_grid_coord);
        vec3 probe_position = GridPositionToWorldPosition(probe_grid_coord);
        vec3 probe_to_point = P - probe_position + (N + 3.0 * V) * PROBE_NORMAL_BIAS;
        vec3 dir = normalize(-probe_to_point);
        
        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight = 1.0;
        
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
    vec4 albedo = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    vec4 normal = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);

    vec4 tangents_buffer = SampleGBuffer(gbuffer_tangents_texture, texcoord);

    vec3 tangent = UnpackNormalVec2(tangents_buffer.xy);
    vec3 bitangent = UnpackNormalVec2(tangents_buffer.zw);

    // vec4 bitangent = vec4(DecodeNormal(SampleGBuffer(gbuffer_bitangents_texture, texcoord)), 1.0);
    float depth = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(scene.projection * scene.view), texcoord, depth);
    vec4 material = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */
    
    bool perform_lighting = albedo.a > 0.0;
    
    vec3 albedo_linear = albedo.rgb;
	vec3 result = vec3(0.0);

    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);
    vec3 R = normalize(reflect(-V, N));
    
    float ao = 1.0;
    vec3 irradiance = vec3(0.0);
    vec4 reflections = vec4(0.0);
    vec3 ibl = vec3(0.0);
    
    if (perform_lighting) {
        const vec4 ssao_data = SampleEffectPre(0, v_texcoord0, vec4(1.0));
        ao = ssao_data.a * material.a;

        const float roughness = material.r;
        const float metalness = material.g;
        
        float NdotV = max(0.0001, dot(N, V));

        const vec3 diffuse_color = albedo_linear * (1.0 - metalness);

        // const float material_reflectance = 0.5;
        // const float reflectance = 0.16 * material_reflectance * material_reflectance; // dielectric reflectance
        
        
        // Index of refraction for common dielectrics. Corresponds to f0 4%
        const float IOR = 1.5;

        // Reflectance of the surface when looking straight at it along the negative normal
        const vec3 reflectance = vec3(pow(IOR - 1.0, 2.0) / pow(IOR + 1.0, 2.0));
        vec3 F0 = albedo_linear * metalness + (reflectance * (1.0 - metalness));
    
        // F0 = mix(F0, albedo_linear, metalness);

        //const vec3 F0 = albedo_linear * (1.0 - metalness);//albedo_linear.rgb * metalness + (reflectance * (1.0 - metalness));
        const vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));
        const vec3 F = SchlickFresnelRoughness(F0, roughness, NdotV);
        
        const vec2 AB = BRDFMap(roughness, NdotV);
        const vec3 dfg = F * AB.x + AB.y;
        
        const vec3 energy_compensation = 1.0 + F0 * ((1.0 / max(dfg.y, 0.00001)) - 1.0);
        const float perceptual_roughness = sqrt(roughness + HYP_CUBEMAP_MIN_ROUGHNESS);

#if HYP_ENV_PROBE_ENABLED
        if (scene.environment_texture_usage != 0) {
            const uint probe_index = scene.environment_texture_index;
            EnvProbe probe = env_probes[probe_index];

            if (probe.texture_index != ~0u) {
                const uint probe_texture_index = max(0, min(probe.texture_index, HYP_MAX_BOUND_ENV_PROBES));
                const int num_levels = EnvProbeGetNumLevels(gbuffer_sampler, env_probe_textures[probe_texture_index]);
                const float lod = float(num_levels) * perceptual_roughness * (2.0 - perceptual_roughness);

                ibl = EnvProbeSample(
                    gbuffer_sampler,
                    env_probe_textures[probe_texture_index],
                    bool(probe.flags & HYP_ENV_PROBE_PARALLAX_CORRECTED)
                        ? EnvProbeCoordParallaxCorrected(probe, position.xyz, R)
                        : R,
                    lod
                ).rgb;
            }
        }
#endif

#if HYP_VCT_ENABLED
        if (IsRenderComponentEnabled(HYP_RENDER_COMPONENT_VCT)) {
            vec4 vct_specular = ConeTraceSpecular(position.xyz, N, R, roughness);
            vec4 vct_diffuse = ConeTraceDiffuse(position.xyz, N, T, B, roughness);

#if HYP_VCT_INDIRECT_ENABLED
            irradiance  = vct_diffuse.rgb;
#endif

#if HYP_VCT_REFLECTIONS_ENABLED
            reflections = vct_specular;
#endif
        }
#endif

#if HYP_SSR_ENABLED
        if (bool(deferred_params.flags & DEFERRED_FLAG_SSR_ENABLED)) {
            vec4 screen_space_reflections = Texture2D(HYP_SAMPLER_LINEAR, ssr_blur_vert, texcoord);
            screen_space_reflections.rgb = pow(screen_space_reflections.rgb, vec3(2.2));
            reflections = mix(reflections, screen_space_reflections, screen_space_reflections.a);
        }
#endif

        vec3 light_color = vec3(1.0);

        // TEMP; will be under a compiler conditional.
        // quick and dirty hack to get not-so accurate indirect light for the scene --
        // sample lowest mipmap of cubemap, if we have it.
        // later, replace this will spherical harmonics.
#if HYP_ENV_PROBE_ENABLED
        if (scene.environment_texture_usage != 0) {
            const uint probe_index = scene.environment_texture_index;
            EnvProbe probe = env_probes[probe_index];

            if (probe.texture_index != ~0u) {
                const uint probe_texture_index = max(0, min(probe.texture_index, HYP_MAX_BOUND_ENV_PROBES));

                const int num_levels = EnvProbeGetNumLevels(gbuffer_sampler, env_probe_textures[probe_texture_index]);

                vec4 env_probe_irradiance = EnvProbeSample(gbuffer_sampler, env_probe_textures[probe_texture_index], N, float(num_levels - 1));

                irradiance += env_probe_irradiance.rgb * env_probe_irradiance.a;
            }
        }
#endif

        const vec3 kD = (1.0 - F) * (1.0 - metalness);

        vec3 Fd = diffuse_color * irradiance / HYP_FMATH_PI;  //(diffuse_color * (irradiance * IRRADIANCE_MULTIPLIER) * (1.0 - dfg) * ao);

        // vec3 Fd = diffuse_color * irradiance * (vec3(1.0) - dfg) * ao;

        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        // const vec3 bent_normal = (inverse(scene.view) * vec4(DecodeNormal(vec4(ssao_data.xyz, 1.0)).xyz, 0.0)).xyz;//ssao_data.xyz;
        // vec3 specular_ao = vec3(SpecularAO_Cones(bent_normal, ao, perceptual_roughness, R));
        specular_ao *= energy_compensation;

        vec3 Fr = dfg * ibl * specular_ao;

        vec3 multibounce = GTAOMultiBounce(ao, diffuse_color);
        Fd *= multibounce;
        Fr *= multibounce;

        Fr *= exposure * IBL_INTENSITY;
        Fd *= exposure * IBL_INTENSITY;

        reflections.rgb *= specular_ao;
        Fr = (Fr * (1.0 - reflections.a)) + (dfg * reflections.rgb);

        result = kD * Fd + Fr;

        result = CalculateFogLinear(vec4(result, 1.0), vec4(vec3(0.7, 0.8, 1.0), 1.0), position.xyz, scene.camera_position.xyz, (scene.camera_near + scene.camera_far) * 0.5, scene.camera_far).rgb;
    } else {
        result = albedo.rgb;
    }

#if SSAO_DEBUG
    result = vec3(ao);
#endif

    // output_color = vec4(0.0);
    output_color = vec4(result, 1.0);


    // output_color.rgb = vec3(float(depth < 0.95)); //vec3(LinearDepth(scene.projection, SampleGBuffer(gbuffer_depth_texture, v_texcoord0).r));
    //output_color = ScreenSpaceReflection(material.r);
}
