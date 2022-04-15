#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;
layout(location=2) in flat vec3 v_light_direction;
layout(location=3) in flat vec3 v_camera_position;

layout(location=0) out vec4 output_color;
layout(location=1) out vec4 output_normals;
layout(location=2) out vec4 output_positions;

layout(set = 1, binding = 0) uniform sampler2D gbuffer_albedo;
layout(set = 1, binding = 1) uniform sampler2D gbuffer_normals;
layout(set = 1, binding = 2) uniform sampler2D gbuffer_positions;


vec3 GetShadowCoord(mat4 shadow_matrix, vec3 pos) {
  vec4 shadow_position = shadow_matrix * vec4(pos, 1.0);
  
  shadow_position *= vec4(0.5);
  shadow_position += vec4(0.5);
  shadow_position.xyz /= shadow_position.w;
  
  return shadow_position.xyz;
}

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}


float CookTorranceG(float NdotL, float NdotV, float LdotH, float NdotH)
{
    return min(1, 2 * (NdotH / LdotH) * min(NdotL, NdotV));
}

vec3 SchlickFresnel(vec3 f0, vec3 f90, float u)
{
    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

vec3 mon2lin(vec3 x)
{
    return vec3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

void main()
{
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo;
    vec4 normal;
    vec4 position;
    
    albedo = texture(gbuffer_albedo, texcoord);
    normal = texture(gbuffer_normals, texcoord) * 2.0 - 1.0;
    position = texture(gbuffer_positions, texcoord);
    
    float metallic = 0.5;
    float roughness = 0.5;
    
    vec3 N = normal.xyz;
    vec3 L = v_light_direction;
    vec3 V = normalize(v_camera_position.xyz - position.xyz);
    
    float NdotL = max(0.001, dot(N, L));
    float NdotV = max(0.001, dot(N, V));
    vec3 H = normalize(L + V);
    float NdotH = max(0.001, dot(N, H));
    float LdotH = max(0.001, dot(L, H));
    float VdotH = max(0.001, dot(V, H));
    float HdotV = max(0.001, dot(H, V));
    
	/*float LdotX = dot(L, X);
	float LdotY = dot(L, Y);
	float VdotX = dot(V, X);
	float VdotY = dot(V, Y);
	float HdotX = dot(H, X);
	float HdotY = dot(H, Y);*/

    
    
    /*
    vec3 Cdlin = mon2lin(albedo.rgb);
	vec3 result;
	float materialReflectance = 0.5;
	float reflectance = 0.16 * materialReflectance * materialReflectance; // dielectric reflectance
	vec3 F0 = Cdlin.rgb * metallic + (reflectance * (1.0 - metallic));
    // surface
    vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));
    float _D = DistributionGGX(N, H, roughness); // GTR2_aniso(NdotH, HdotX, HdotY, ax, ay)
	vec3 _Fr = (_D * CookTorranceG(NdotL, NdotV, LdotH, NdotH)) * SchlickFresnel(F0, F90, LdotH);
	// Burley 2012, "Physically-Based Shading at Disney"
    //float f90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
    vec3 light_scatter = SchlickFresnel(vec3(1.0), F90, NdotL);
    vec3 view_scatter  = SchlickFresnel(vec3(1.0), F90, NdotV);
    vec3 diffuse = Cdlin.rgb * (1.0 - metallic) * (light_scatter * view_scatter * (1.0 / $PI));*/
    
    output_color = vec4(albedo.rgb * vec3(NdotL), 1.0);
    output_normals = normal;
    output_positions = position;
}