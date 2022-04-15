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


layout(set = 6, binding = 0) uniform samplerCube cubemap_textures[];

layout(std430, set = 2, binding = 0, row_major) uniform SceneDataBlock {
    mat4 view;
    mat4 projection;
    vec4 camera_position;
    vec4 light_direction;
    
    uint environment_texture_index;
    uint environment_texture_usage;
    uint _padding1;
    uint _padding2;
} scene;

bool HasEnvironmentTexture(uint index)
{
    return bool(scene.environment_texture_usage & (1 << index));
}


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

vec2 BRDFMap(float NdotV, float metallic)
{
    const vec4 c0 = vec4(-1, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1, 0.0425, 1.04, -0.04);
    vec4 r = metallic * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;

    return vec2(-1.04, 1.04) * a004 + r.zw;
}

float sqr(float x)
{
    return x * x;
}

/* From Google Filament */

/**
 * Approximates acos(x) with a max absolute error of 9.0x10^-3.
 * Valid in the range -1..1.
 */
float acosFast(float x)
{
    // Lagarde 2014, "Inverse trigonometric functions GPU optimization for AMD GCN architecture"
    // This is the approximation of degree 1, with a max absolute error of 9.0x10^-3
    float y = abs(x);
    float p = -0.1565827 * y + 1.570796;
    p *= sqrt(1.0 - y);
    return x >= 0.0 ? p : PI - p;
}

/**
 * Approximates acos(x) with a max absolute error of 9.0x10^-3.
 * Valid only in the range 0..1.
 */
float acosFastPositive(float x)
{
    float p = -0.1565827 * x + 1.570796;
    return p * sqrt(1.0 - x);
}

float sphericalCapsIntersection(float cosCap1, float cosCap2, float cosDistance)
{
    // Oat and Sander 2007, "Ambient Aperture Lighting"
    // Approximation mentioned by Jimenez et al. 2016
    float r1 = acosFastPositive(cosCap1);
    float r2 = acosFastPositive(cosCap2);
    float d  = acosFast(cosDistance);

    // We work with cosine angles, replace the original paper's use of
    // cos(min(r1, r2)) with max(cosCap1, cosCap2)
    // We also remove a multiplication by 2 * PI to simplify the computation
    // since we divide by 2 * PI in computeBentSpecularAO()

    if (min(r1, r2) <= max(r1, r2) - d) {
        return 1.0 - max(cosCap1, cosCap2);
    } else if (r1 + r2 <= d) {
        return 0.0;
    }

    float delta = abs(r1 - r2);
    float x = 1.0 - clamp((d - delta) / max(r1 + r2 - delta, 1e-4), 0.0, 1.0);
    // simplified smoothstep()
    float area = sqr(x) * (-2.0 * x + 3.0);
    return area * (1.0 - max(cosCap1, cosCap2));
}

float SpecularAO_Cones(vec3 bentNormal, float visibility, float roughness, vec3 shading_reflected)
{
    // Jimenez et al. 2016, "Practical Realtime Strategies for Accurate Indirect Occlusion"

    // aperture from ambient occlusion
    float cosAv = sqrt(1.0 - visibility);
    // aperture from roughness, log(10) / log(2) = 3.321928
    float cosAs = exp2(-3.321928 * sqr(roughness));
    // angle betwen bent normal and reflection direction
    float cosB  = dot(bentNormal, shading_reflected);

    // Remove the 2 * PI term from the denominator, it cancels out the same term from
    // sphericalCapsIntersection()
    float ao = sphericalCapsIntersection(cosAv, cosAs, cosB) / (1.0 - cosAs);
    // Smoothly kill specular AO when entering the perceptual roughness range [0.1..0.3]
    // Without this, specular AO can remove all reflections, which looks bad on metals
    return mix(1.0, ao, smoothstep(0.01, 0.09, roughness));
}

float SpecularAO_Lagarde(float NoV, float visibility, float roughness)
{
    // Lagarde and de Rousiers 2014, "Moving Frostbite to PBR"
    return clamp(pow(NoV + visibility, exp2(-16.0 * roughness - 1.0)) - 1.0 + visibility, 0.0, 1.0);
}

/* Begin main shader program */

const float IBL_INTENSITY = 4000.0;

void main()
{
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo;
    vec4 normal;
    vec4 position;
    
    albedo = texture(gbuffer_albedo, texcoord);
    normal = texture(gbuffer_normals, texcoord) * 2.0 - 1.0;
    position = texture(gbuffer_positions, texcoord);
    
    float metallic = 0.1;
    float roughness = 0.6;
    
    vec3 N = normal.xyz;
    vec3 L = v_light_direction;
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
    
    vec3 irradiance = vec3(0.1); /* TODO */
    
    vec3 ibl = HasEnvironmentTexture(0)
        ? textureLod(cubemap_textures[scene.environment_texture_index], R, lod).rgb
        : vec3(1.0);
    

    float ao = 1.0;
    vec3 shadowColor = vec3(1.0);
    vec3 light_color = vec3(1.0);
    float light_intensity = 200000.0;
    
    
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
	vec3 specular = D * G * F;
    
    vec3 light_scatter = SchlickFresnel(vec3(1.0), F90, NdotL);
    vec3 view_scatter  = SchlickFresnel(vec3(1.0), F90, NdotV);
    vec3 diffuse = Cdlin.rgb * (1.0 - metallic) * (light_scatter * view_scatter * (1.0 / PI));

	vec3 surface = diffuse + specular * energyCompensation;
	surface *= light_color * (/*light.attenuation*/1.0 * NdotL * ao * shadowColor);
	surface *= exposure * light_intensity;
    
	result += surface;
    

    
    output_color = vec4(result, 1.0);
    output_normals = normal;
    output_positions = position;
}