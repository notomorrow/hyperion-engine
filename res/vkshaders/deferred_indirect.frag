#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;
layout(location=1) out vec4 output_normals;
layout(location=2) out vec4 output_positions;

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 39) uniform texture2D ssr_result;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 41) uniform texture2D ssao_gi_result;
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

#define HYP_VCT_REFLECTIONS_ENABLED 1
#define HYP_VCT_INDIRECT_ENABLED 1

#ifdef VCT_ENABLED_TEXTURE
    #include "include/vct/cone_trace.inc"
#endif

/* Begin main shader program */

#define IBL_INTENSITY 7500.0
#define IRRADIANCE_MULTIPLIER 1.0

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

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

    const vec4 ssao_data = Texture2D(HYP_SAMPLER_LINEAR, ssao_gi_result, v_texcoord0);
    ao = min(mix(1.0, ssao_data.a, bool(deferred_params.flags & DEFERRED_FLAGS_HBAO_ENABLED)), material.a);
    
#if defined(VCT_ENABLED_TEXTURE) || defined(VCT_ENABLED_SVO)
    vec4 vct_specular = vec4(0.0);
    vec4 vct_diffuse = vec4(0.0);
#endif

    if (perform_lighting) {
        const float roughness = material.r;
        const float metalness = material.g;

        float NdotV = max(0.0001, dot(N, V));

        const vec3 diffuse_color = CalculateDiffuseColor(albedo_linear, metalness);
        const vec3 F0 = CalculateF0(albedo_linear, metalness);

        F = CalculateFresnelTerm(F0, roughness, NdotV);

        const float perceptual_roughness = sqrt(roughness);
        const vec3 dfg = CalculateDFG(F, perceptual_roughness, NdotV);
        const vec3 E = CalculateE(F0, dfg);
        const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

#ifdef VCT_ENABLED_TEXTURE
        if (IsRenderComponentEnabled(HYP_RENDER_COMPONENT_VCT)) {
            vct_specular = ConeTraceSpecular(position.xyz, N, R, roughness);
            vct_diffuse = ConeTraceDiffuse(position.xyz, N, T, B, roughness);

#if HYP_VCT_INDIRECT_ENABLED
            irradiance = vct_diffuse.rgb;
#endif

#if HYP_VCT_REFLECTIONS_ENABLED
            reflections = vct_specular;
#endif
        }
#endif

#ifdef ENV_PROBE_ENABLED
        ibl = CalculateEnvProbeReflection(deferred_params, position.xyz, N, R, scene.camera_position.xyz, perceptual_roughness);
#endif

#ifdef SSR_ENABLED
        CalculateScreenSpaceReflection(deferred_params, texcoord, depth, reflections);
#endif

#ifdef ENV_PROBE_ENABLED
        CalculateEnvProbeIrradiance(deferred_params, position.xyz, N, irradiance);
#endif

#ifdef RT_ENABLED
        CalculateRaytracingReflection(deferred_params, texcoord, reflections);

        irradiance += DDGISampleIrradiance(position.xyz, N, V).rgb;
#endif

        CalculateHBILIrradiance(deferred_params, ssao_data, irradiance);

        vec3 Fd = diffuse_color.rgb * (irradiance * IRRADIANCE_MULTIPLIER) * (1.0 - E) * ao;

        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        specular_ao *= energy_compensation;

        vec3 Fr = E * ibl;

        Fr *= specular_ao;
        reflections.rgb *= specular_ao;

        vec3 multibounce = GTAOMultiBounce(ao, albedo.rgb);
        Fd *= multibounce;
        Fr *= multibounce;

        // Fr *= exposure * IBL_INTENSITY;
        // Fd *= exposure * IBL_INTENSITY;

        IntegrateReflections(Fr, E, reflections);

        result = Fd + Fr;

        vec4 final_result = vec4(result, 1.0);
        ApplyFog(position.xyz, final_result);
        result = final_result.rgb;
    } else {
        result = albedo.rgb;
    }

    #ifdef ENV_PROBE_ENABLED

    #define PROBE_CAGE_VIEW_RANGE 800.0

    // const vec3 grid_center = vec3(0.5) * (vec3(1.0) * vec3(cage_size)) - 0.5;

    vec3 relative_position = position.xyz - scene.camera_position.xyz;

    const ivec3 cage_size = textureSize(sampler3D(sh_clipmaps[0], sampler_linear), 0);

    vec3 camera_coord = fract((scene.camera_position.xyz / PROBE_CAGE_VIEW_RANGE) + 0.5);
    ivec3 camera_position_in_grid = ivec3(camera_coord * vec3(cage_size) - 0.5);

    vec3 abs_position_coord = fract((position.xyz / PROBE_CAGE_VIEW_RANGE) + 0.5);
    ivec3 abs_position_in_grid = ivec3(abs_position_coord * vec3(cage_size) - 0.5);

    vec3 cage_size_world = vec3(cage_size) * PROBE_CAGE_VIEW_RANGE;

    vec3 cage_coord = (relative_position / PROBE_CAGE_VIEW_RANGE) + 0.5;//(((relative_position) + vec3(cage_size_world) * 0.5) / cage_size_world) - 0.5;
    ivec3 cage_coord_pixel = ivec3(floor(cage_coord * vec3(cage_size) - 0.5));

    vec3 texel_center = (vec3(cage_coord_pixel) + 0.5) * PROBE_CAGE_VIEW_RANGE;
    const vec3 texel_center_diff = (position.xyz / PROBE_CAGE_VIEW_RANGE) - vec3(cage_coord_pixel);

    const vec3 size_one_block_world = (vec3(1.0) / vec3(cage_size)) * PROBE_CAGE_VIEW_RANGE;


    vec3 position_fract = vec3(0.0);//((texel_center_diff / size_one_block_world)); //fract(cage_coord * vec3(cage_size) - 0.5);
    const vec3 camera_fract = ((fract(cage_coord * vec3(cage_size) - 0.5)) * 2.0 - 1.0) * (vec3(1.0) / vec3(cage_size));//(fract((scene.camera_position.xyz / PROBE_CAGE_VIEW_RANGE))) * (vec3(1.0) / vec3(cage_size));
    position_fract += camera_fract;
    // position_fract *= (vec3(1.0) / vec3(cage_size));

    // const float view_depth = ReconstructViewSpacePositionFromDepth(inverse(scene.projection), texcoord, depth).z;//ViewDepth(depth, scene.camera_near, scene.camera_far);
    // const float view_depth_clamped = saturate(view_depth / MAX_DISTANCE);

    // vec3 uv_clipmap = vec3(texcoord, view_depth_clamped);

    const float cos_a0 = HYP_FMATH_PI;
    const float cos_a1 = (2.0 * HYP_FMATH_PI) / 3.0;
    const float cos_a2 = HYP_FMATH_PI * 0.25;

    float bands[9] = ProjectSHBands(-N);
    bands[0] *= cos_a0;
    bands[1] *= cos_a1;
    bands[2] *= cos_a1;
    bands[3] *= cos_a1;
    bands[4] *= cos_a2;
    bands[5] *= cos_a2;
    bands[6] *= cos_a2;
    bands[7] *= cos_a2;
    bands[8] *= cos_a2;

    irradiance = vec3(0.0);

    for (int i = 0; i < 9; i++) {
        // irradiance += Texture3D(sampler_linear, sh_clipmaps[i], (((floor(cage_coord_pixel) - (vec3(cage_size) * 0.5)) + 0.5) / vec3(cage_size)) + 0.5 ).rgb * bands[i];
        irradiance += Texture3D(sampler_linear, sh_clipmaps[i], cage_coord).rgb * bands[i];//((vec3(cage_coord_pixel) + 0.5) / vec3(cage_size)) + position_fract ).rgb * bands[i];
    }

    irradiance = max(irradiance, vec3(0.0));
    irradiance /= HYP_FMATH_PI;

    irradiance *= 4.0;

    // irradiance = Texture3D(sampler_nearest, sh_clipmaps[0], cage_coord).rgb;//(vec3(abs_position_in_grid - camera_position_in_grid) + 0.5) / vec3(cage_size) + 0.5).rgb;

    #endif

    output_color = vec4(irradiance.rgb, 1.0);
}