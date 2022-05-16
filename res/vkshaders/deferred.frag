#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;
layout(location=1) out vec4 output_normals;
layout(location=2) out vec4 output_positions;

#include "include/gbuffer.inc"

layout(set = 1, binding = 8) uniform sampler2D filter_ssao[1];
layout(set = 6, binding = 0) uniform samplerCube cubemap_textures[];

//layout(set = 8, binding = 0, rgba16f) readonly uniform image3D voxel_image;

#include "include/scene.inc"
#include "include/brdf.inc"
#include "include/tonemap.inc"

#include "include/vct/cone_trace.inc"

vec3 GetShadowCoord(mat4 shadow_matrix, vec3 pos)
{
    vec4 shadow_position = shadow_matrix * vec4(pos, 1.0);
  
    shadow_position *= vec4(0.5);
    shadow_position += vec4(0.5);
    shadow_position.xyz /= shadow_position.w;
  
    return shadow_position.xyz;
}

/* Begin main shader program */

#define IBL_INTENSITY 5000.0
#define DIRECTIONAL_LIGHT_INTENSITY 40000.0
#define IRRADIANCE_MULTIPLIER 8.0
#define SSAO_DEBUG 0

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

vec3 SpecularDFG(vec2 AB, vec3 F0)
{
    return mix(AB.xxx, AB.yyy, F0);
}

vec3 GtaoMultiBounce(float visibility, const vec3 albedo) {
    // Jimenez et al. 2016, "Practical Realtime Strategies for Accurate Indirect Occlusion"
    vec3 a =  2.0404 * albedo - 0.3324;
    vec3 b = -4.7951 * albedo + 0.6417;
    vec3 c =  2.7552 * albedo + 0.6903;

    return max(vec3(visibility), ((visibility * a + b) * visibility + c) * visibility);
}

void main()
{
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo   = texture(gbuffer_albedo_texture, texcoord);
    vec4 normal   = texture(gbuffer_normals_texture, texcoord);
    vec4 position = texture(gbuffer_positions_texture, texcoord);
    vec4 material = texture(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */
    
    bool perform_lighting = albedo.a > 0.0;
    
    vec3 albedo_linear = albedo.rgb;
	vec3 result = vec3(0.0);

    /* Physical camera */
    float aperture = 16.0;
    float shutter = 1.0/125.0;
    float sensitivity = 100.0;
    float ev100 = log2((aperture * aperture) / shutter * 100.0f / sensitivity);
    float exposure = 1.0f / (1.2f * pow(2.0f, ev100));
    
    
	vec3 o = scene.camera_position.xyz,
         d = (inverse(scene.projection * scene.view) * vec4(v_texcoord0 * 2.0 - 1.0, 1.0, 1.0)).xyz;

    vec3 N = normal.xyz;
    vec3 L = normalize(scene.light_direction.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);
    vec3 R = normalize(reflect(-V, N));
    vec3 H = normalize(L + V);
    
    float ao = 1.0;
    
    if (perform_lighting) {
        ao = texture(filter_ssao[0], texcoord).r * material.a;

        const float roughness = material.r;
        const float metalness = material.g;
        
        float NdotL = max(0.0001, dot(N, L));
        float NdotV = max(0.0001, dot(N, V));
        float NdotH = max(0.0001, dot(N, H));
        float LdotH = max(0.0001, dot(L, H));
        float VdotH = max(0.0001, dot(V, H));
        float HdotV = max(0.0001, dot(H, V));

        
        const float material_reflectance = 0.5;
        const float reflectance = 0.16 * material_reflectance * material_reflectance; // dielectric reflectance
        vec3 F0 = albedo_linear.rgb * metalness + (reflectance * (1.0 - metalness));
        
        vec3 energy_compensation = vec3(1.0);
        float perceptual_roughness = sqrt(roughness);
        float lod = 12.0 * perceptual_roughness * (2.0 - perceptual_roughness);
        
        vec3 AB = BRDFMap(albedo.rgb, roughness, NdotV);
        
        vec3 irradiance = vec3(0.0);

        vec3 ibl = HasEnvironmentTexture(0)
            ? textureLod(cubemap_textures[scene.environment_texture_index], R, lod).rgb
            : vec3(0.0);
        
        vec4 vct_specular = ConeTraceSpecular(position.xyz, N, R, roughness);
        vec4 vct_diffuse  = ConeTraceDiffuse(position.xyz, N, vec3(0.0), vec3(0.0), roughness);
        irradiance = vct_diffuse.rgb * IRRADIANCE_MULTIPLIER;

        vec3 shadow = vec3(1.0);
        vec3 light_color = vec3(1.0);
        
        // ibl
        vec3 dfg = mix(vec3(AB.x), vec3(AB.y), F0);
        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        vec3 radiance = dfg * ibl * specular_ao;
        result = (albedo_linear * irradiance * (1.0 - dfg) * ao) + radiance;
        result *= exposure * IBL_INTENSITY;

        result = (result * (1.0 - vct_specular.a)) + (dfg * vct_specular.rgb);


        // surface
        vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));
        float D = DistributionGGX(N, H, roughness);
        float G = CookTorranceG(NdotL, NdotV, LdotH, NdotH);
        vec3 F = SchlickFresnel(F0, F90, LdotH);
        vec3 specular = D * G * F;
        //vec3 fresnel = (F * G * D) / (4.0 * NdotL * NdotV);
        
        vec3 light_scatter = SchlickFresnel(vec3(1.0), F90, NdotL);
        vec3 view_scatter  = SchlickFresnel(vec3(1.0), F90, NdotV);
        vec3 diffuse = albedo_linear * (1.0 - metalness) * (light_scatter * view_scatter * (1.0 / PI));

        // Chan 2018, "Material Advances in Call of Duty: WWII"
        //float micro_shadow_aperture = inversesqrt(1.0 - ao);
        //float micro_shadow = clamp(NdotL * micro_shadow_aperture, 0.0, 1.0);
        //float micro_shadow_sqr = micro_shadow * micro_shadow;

        result += (specular + diffuse * energy_compensation) * ((exposure * DIRECTIONAL_LIGHT_INTENSITY) * NdotL * ao);
    } else {
        result = albedo.rgb;
    }

#if SSAO_DEBUG
    result = vec3(ao);
#endif

    output_color     = vec4(Tonemap(result), 1.0);
    output_normals   = normal;
    output_positions = position;
}