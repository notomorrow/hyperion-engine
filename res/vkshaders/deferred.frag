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

#define IBL_INTENSITY 12000.0

void main()
{
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo;
    vec4 normal;
    vec4 position;
    
    albedo = texture(gbuffer_albedo_texture, texcoord);
    normal = texture(gbuffer_normals_texture, texcoord);
    position = texture(gbuffer_positions_texture, texcoord);
    
    float metallic = 0.05;
    float roughness = 0.5;
    
    vec3 N = normal.xyz;
    vec3 L = normalize(scene.light_direction.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);
    vec3 R = reflect(-V, N);
    vec3 H = normalize(L + V);
    
    float NdotL = max(0.001, dot(N, L));
    float NdotV = max(0.001, dot(N, V));
    float NdotH = max(0.001, dot(N, H));
    float LdotH = max(0.001, dot(L, H));
    float VdotH = max(0.001, dot(V, H));
    float HdotV = max(0.001, dot(H, V));
    

    /* Physical camera */
    float aperture = 16.0;
	float shutterSpeed = 1.0/125.0;
	float sensitivity = 100.0;
	float ev100 = log2((aperture * aperture) / shutterSpeed * 100.0f / sensitivity);
	float exposure = 1.0f / (1.2f * pow(2.0f, ev100));
    
    
    vec3 Cdlin = mon2lin(albedo.rgb);
	vec3 result;
    
	float materialReflectance = 0.5;
	float reflectance = 0.16 * materialReflectance * materialReflectance; // dielectric reflectance
	vec3 F0 = Cdlin.rgb * metallic + (reflectance * (1.0 - metallic));
    
    vec3 energyCompensation = vec3(1.0);
    float perceptualRoughness = sqrt(roughness);
    float lod = 12.0 * perceptualRoughness * (2.0 - perceptualRoughness);
    
    vec2 AB = vec2(1.0, 1.0) - BRDFMap(NdotV, perceptualRoughness);
    
    vec3 irradiance = vec3(1.0); /* TODO */
    
    vec3 ibl = HasEnvironmentTexture(0)
        ? textureLod(cubemap_textures[scene.environment_texture_index], R, lod).rgb
        : vec3(1.0);
    

    float ao = texture(filter_ssao, texcoord).r;
    vec3 shadowColor = vec3(1.0);
    vec3 light_color = vec3(1.0);
    float light_intensity = 100000.0;
    
    
    vec3 specularDFG = mix(vec3(AB.x), vec3(AB.y), F0);
    vec3 specularSingleBounceAO = vec3(SpecularAO_Cones(N, ao, roughness, R));
	vec3 radiance = specularDFG * ibl * specularSingleBounceAO;
	vec3 _Fd = Cdlin.rgb * irradiance * (1.0 - specularDFG) * ao;//todo diffuse brdf for cloth
	result = _Fd + radiance;
	
	result *= IBL_INTENSITY * exposure;
    
    
    
    // surface
    vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));
    float D = DistributionGGX(N, H, roughness);
    float G = CookTorranceG(NdotL, NdotV, LdotH, NdotH);
    vec3 F = SchlickFresnel(F0, F90, LdotH);
	vec3 specular = D * G * F * NdotL;
    
    vec3 light_scatter = SchlickFresnel(vec3(1.0), F90, NdotL);
    vec3 view_scatter  = SchlickFresnel(vec3(1.0), F90, NdotV);
    vec3 diffuse = Cdlin.rgb * (1.0 - metallic) * (light_scatter * view_scatter * (1.0 / PI));

	vec3 surface = diffuse + specular * energyCompensation;
	surface *= light_color * (/*light.attenuation*/1.0 * NdotL * ao * shadowColor);
	surface *= exposure * light_intensity;
    
	result += surface;
    

    
    output_color = vec4(Tonemap(result), 1.0);
    output_normals = normal;
    output_positions = position;
}