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

layout(set = 1, binding = 8) uniform sampler2D filter_ssao;

layout(set = 6, binding = 0) uniform samplerCube cubemap_textures[];

#include "include/scene.inc"
#include "include/brdf.inc"
#include "include/tonemap.inc"

vec3 GetShadowCoord(mat4 shadow_matrix, vec3 pos) {
  vec4 shadow_position = shadow_matrix * vec4(pos, 1.0);
  
  shadow_position *= vec4(0.5);
  shadow_position += vec4(0.5);
  shadow_position.xyz /= shadow_position.w;
  
  return shadow_position.xyz;
}

/* Begin main shader program */

#define IBL_INTENSITY 7000.0
#define DIRECTIONAL_LIGHT_INTENSITY 100000.0
#define GI_INTENSITY 20.0
#define VCT_ENABLED 0

#if VCT_ENABLED
#include "include/voxel/vct.inc"
#endif


void main()
{
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo;
    vec4 normal;
    vec4 position;
    
    albedo = texture(gbuffer_albedo_texture, texcoord);
    normal = texture(gbuffer_normals_texture, texcoord);
    position = texture(gbuffer_positions_texture, texcoord);
    
    bool perform_lighting = albedo.a > 0.0;
    
    vec3 albedo_linear = albedo.rgb;//mon2lin(albedo.rgb);
	vec3 result;

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
    vec3 R = reflect(-V, N);
    vec3 H = normalize(L + V);
    
    if (perform_lighting) {
        float metallic = 0.01;
        float roughness = 0.8;
        
        float NdotL = max(0.0001, dot(N, L));
        float NdotV = max(0.0001, dot(N, V));
        float NdotH = max(0.0001, dot(N, H));
        float LdotH = max(0.0001, dot(L, H));
        float VdotH = max(0.0001, dot(V, H));
        float HdotV = max(0.0001, dot(H, V));

        
        float materialReflectance = 0.5;
        float reflectance = 0.16 * materialReflectance * materialReflectance; // dielectric reflectance
        vec3 F0 = albedo_linear.rgb * metallic + (reflectance * (1.0 - metallic));
        
        vec3 energy_compensation = vec3(1.0);
        float perceptual_roughness = sqrt(roughness);
        float lod = 12.0 * perceptual_roughness * (2.0 - perceptual_roughness);
        
        vec2 AB = vec2(1.0, 1.0) - BRDFMap(NdotV, perceptual_roughness);
        
        vec3 irradiance = vec3(0.0);
        vec3 ibl = HasEnvironmentTexture(0)
            ? textureLod(cubemap_textures[scene.environment_texture_index], R, lod).rgb
            : vec3(0.0);
        
#if VCT_ENABLED
        vec3 aabb_max = vec3(64.0) + vec3(0.0, 0.0 , 5.0);
        vec3 aabb_min = vec3(-64.0) + vec3(0.0, 0.0, 5.0);
        
        //vec3 vct_irradiance = vec3(0.0);
        
        //for (int i = 0; i < 16; i++) {
        irradiance += voxelTraceCone(position.xyz, N, aabb_max, aabb_min, lod, 0.3).rgb;
        //}
        
        //irradiance += vct_irradiance / 16.0;
        
        ibl += voxelTraceCone(position.xyz, R, aabb_max, aabb_min, lod, 0.4).rgb;
#endif

        float ao = texture(filter_ssao, texcoord).r;
        vec3 shadow = vec3(1.0);
        vec3 light_color = vec3(1.0);
        
        
        vec3 dfg = mix(vec3(AB.x), vec3(AB.y), F0);
        vec3 specular_ao = vec3(SpecularAO_Cones(N, ao, roughness, R));
        vec3 radiance = dfg * ibl * specular_ao;
        result = (albedo_linear * irradiance * (1.0 - dfg) * ao) + radiance;
        result *= exposure * IBL_INTENSITY;

        // surface
        vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));
        float D = DistributionGGX(N, H, roughness);
        float G = CookTorranceG(NdotL, NdotV, LdotH, NdotH);
        vec3 F = SchlickFresnel(F0, F90, LdotH);
        vec3 specular = D * G * F * NdotL;
        
        vec3 light_scatter = SchlickFresnel(vec3(1.0), F90, NdotL);
        vec3 view_scatter  = SchlickFresnel(vec3(1.0), F90, NdotV);
        vec3 diffuse = albedo_linear * (1.0 - metallic) * (light_scatter * view_scatter * (1.0 / PI));

        vec3 surface = diffuse + specular * energy_compensation;
        surface *= light_color * (/*light.attenuation*/1.0 * NdotL * ao * shadow);
        surface *= exposure * DIRECTIONAL_LIGHT_INTENSITY;
        
        result += surface;
    } else {
        result = albedo_linear * DIRECTIONAL_LIGHT_INTENSITY * exposure;
    }

    output_color = vec4(Tonemap(result), 1.0);
    output_normals = normal;
    output_positions = position;
}