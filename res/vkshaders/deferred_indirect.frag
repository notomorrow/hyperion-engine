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

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 45) uniform texture2D rt_radiance_final;

#include "include/env_probe.inc"
#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/PostFXSample.inc"
#include "include/tonemap.inc"

#include "include/scene.inc"
#include "include/PhysicalCamera.inc"

#define HYP_DEFERRED_NO_REFRACTION
#include "./deferred/DeferredLighting.glsl"
#undef HYP_DEFERRED_NO_REFRACTION

vec2 texcoord = v_texcoord0;

#define HYP_VCT_ENABLED 1
#define HYP_VCT_REFLECTIONS_ENABLED 1
#define HYP_VCT_INDIRECT_ENABLED 1
#define HYP_ENV_PROBE_ENABLED 1
#define HYP_SSR_ENABLED 1

#if HYP_VCT_ENABLED
#include "include/vct/cone_trace.inc"
#else
#include "include/voxel/vct.inc"
#endif

/* Begin main shader program */

#define IBL_INTENSITY 40000.0
#define IRRADIANCE_MULTIPLIER 1.0
#define SSAO_DEBUG 0
#define HYP_CUBEMAP_MIN_ROUGHNESS 0.0

#define DEFERRED_FLAG_SSR_ENABLED 0x1

layout(push_constant) uniform DeferredParams
{
    uint flags;
} deferred_params;

#include "include/DDGI.inc"

void main()
{
    vec4 albedo = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    vec4 normal = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);

    vec4 tangents_buffer = SampleGBuffer(gbuffer_tangents_texture, texcoord);

    vec3 tangent = UnpackNormalVec2(tangents_buffer.xy);
    vec3 bitangent = UnpackNormalVec2(tangents_buffer.zw);
    float depth = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(scene.projection), inverse(scene.view), texcoord, depth);
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
    vec3 F = vec3(0.0);

    const vec4 ssao_data = SampleEffectPre(0, v_texcoord0, vec4(1.0));
    ao = ssao_data.a * material.a;

    if (perform_lighting) {
        const float roughness = material.r;
        const float metalness = material.g;
        
        float NdotV = max(0.0001, dot(N, V));

        const vec3 diffuse_color = CalculateDiffuseColor(albedo_linear, metalness);
        const vec3 F0 = CalculateF0(albedo_linear, metalness);
        vec3 F90 = vec3(clamp(dot(F0.rgb, vec3(50.0 * 0.33)), 0.0, 1.0));

        F = CalculateFresnelTerm(F0, roughness, NdotV);

        const float perceptual_roughness = sqrt(roughness);
        const vec3 dfg = CalculateDFG(F, perceptual_roughness, NdotV);
        const vec3 E = CalculateE(F0, dfg);
        
        const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

#if HYP_ENV_PROBE_ENABLED
        if (scene.environment_texture_usage != 0) {
            const uint probe_index = scene.environment_texture_index;
            EnvProbe probe = env_probes[probe_index];

            if (probe.texture_index != ~0u) {
                const uint probe_texture_index = max(0, min(probe.texture_index, HYP_MAX_BOUND_ENV_PROBES));
                const int num_levels = GetNumLevels(gbuffer_sampler, env_probe_textures[probe_texture_index]);
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
        vec4 vct_specular;
        vec4 vct_diffuse;

        if (IsRenderComponentEnabled(HYP_RENDER_COMPONENT_VCT)) {
            vct_specular = ConeTraceSpecular(position.xyz, N, R, roughness);
            vct_diffuse = ConeTraceDiffuse(position.xyz, N, T, B, roughness);

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

                const int num_levels = GetNumLevels(gbuffer_sampler, env_probe_textures[probe_texture_index]);

                vec4 env_probe_irradiance = EnvProbeSample(gbuffer_sampler, env_probe_textures[probe_texture_index], N, float(num_levels - 1));

                // irradiance += env_probe_irradiance.rgb * env_probe_irradiance.a;
            }
        }
#endif

#if 1
        { // RT Radiance
            const int num_levels = GetNumLevels(HYP_SAMPLER_LINEAR, rt_radiance_final);
            const float lod = float(num_levels) * perceptual_roughness * (2.0 - perceptual_roughness);
            vec4 rt_radiance = Texture2DLod(HYP_SAMPLER_LINEAR, rt_radiance_final, texcoord, 0.0);
            reflections = rt_radiance;//mix(reflections, rt_radiance, rt_radiance.a);
        }
#endif

        irradiance += DDGISampleIrradiance(position.xyz, N, V).rgb;


        // TODO: a define for this or parameter
        // GTAO stores indirect light in RGB
        irradiance += ssao_data.rgb;

        vec3 Fd = diffuse_color.rgb * (irradiance * IRRADIANCE_MULTIPLIER) * (1.0 - E) * ao;

        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        specular_ao *= energy_compensation;

        vec3 Fr = E * ibl;
        Fr *= specular_ao;

        reflections.rgb *= specular_ao;

        vec3 multibounce = GTAOMultiBounce(ao, albedo.rgb);
        Fd *= multibounce;
        Fr *= multibounce;

        Fr *= exposure * IBL_INTENSITY;
        Fd *= exposure * IBL_INTENSITY;

        Fr = (Fr * (1.0 - reflections.a)) + (E * reflections.rgb);

        result = Fd + Fr;

        result = CalculateFogLinear(vec4(result, 1.0), vec4(vec3(0.7, 0.8, 1.0), 1.0), position.xyz, scene.camera_position.xyz, (scene.camera_near + scene.camera_far) * 0.5, scene.camera_far).rgb;
    } else {
        result = albedo.rgb;
    }

#if SSAO_DEBUG
    result = vec3(ao);
#endif

    // result = irradiance.rgb;

    output_color = vec4(result, 1.0);
}
