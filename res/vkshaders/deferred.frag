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
#include "include/packing.inc"
#include "include/post_fx.inc"

layout(set = 6, binding = 0) uniform samplerCube cubemap_textures[];

//layout(set = 8, binding = 0, rgba16f) readonly uniform image3D voxel_image;

#include "include/scene.inc"
#include "include/brdf.inc"
#include "include/tonemap.inc"

vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);


#define HYP_VCT_ENABLED 1

#if HYP_VCT_ENABLED
#include "include/vct/cone_trace.inc"
#endif

#include "include/noise.inc"

#define HYP_SHADOW_BIAS 0.005
#define HYP_SHADOW_VARIABLE_BIAS 1
#define HYP_SHADOW_PENUMBRA 1
#define HYP_SHADOW_NUM_SAMPLES 16
#define HYP_SHADOW_PENUMBRA_NUM_SAMPLES 8
#define HYP_SHADOW_PENUMBRA_MIN 0.3
#define HYP_SHADOW_PENUMBRA_MAX 1.0

const mat4 shadow_bias_matrix = mat4( 
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0
);

vec3 GetShadowCoord(int index, vec3 pos)
{
    mat4 shadow_matrix = shadow_bias_matrix * shadow_data.matrices[index].projection * shadow_data.matrices[index].view;
    vec4 shadow_position = shadow_matrix * vec4(pos, 1.0);
    shadow_position.xyz /= shadow_position.w;
  
    return shadow_position.xyz;
}

float GetShadow(int index, vec3 pos, vec2 offset, float NdotL)
{
    vec3 coord = GetShadowCoord(index, pos);
    vec4 shadow_sample = texture(shadow_maps[index], vec2(coord.x, 1.0 - coord.y) + offset);
    float shadow_depth = shadow_sample.r;

    float bias = HYP_SHADOW_BIAS;

#if HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.01);
#endif

    return max(step(coord.z - bias, shadow_depth), 0.0);
}

float AvgBlockerDepthToPenumbra(float light_size, float avg_blocker_depth, float shadow_map_coord_z)
{
    float penumbra = (shadow_map_coord_z - avg_blocker_depth) * light_size / avg_blocker_depth;
    penumbra += HYP_SHADOW_PENUMBRA_MIN;
    penumbra = min(HYP_SHADOW_PENUMBRA_MAX, penumbra);
    return penumbra;
}

float GetShadowContactHardened(int index, vec3 pos, float NdotL)
{
    const vec3 coord = GetShadowCoord(index, pos);

    const float shadow_map_depth = coord.z;

    const float shadow_filter_size = 0.005;
    const float penumbra_filter_size = 0.002;
    const float light_size = 0.25; // affects how quickly shadows become soft

    const float gradient_noise = InterleavedGradientNoise(texcoord * vec2(uvec2(scene.resolution_x, scene.resolution_y)));

    float total_blocker_depth = 0.0;
    float num_blockers = 0.0;

    for (int i = 0; i < HYP_SHADOW_PENUMBRA_NUM_SAMPLES; i++) {
        vec2 filter_uv = VogelDisk(i, HYP_SHADOW_PENUMBRA_NUM_SAMPLES, gradient_noise);
        float blocker_sample = texture(shadow_maps[index], vec2(coord.x, 1.0 - coord.y) + (filter_uv * penumbra_filter_size)).r;

        float is_blocking = float(blocker_sample < shadow_map_depth);

        total_blocker_depth += blocker_sample * is_blocking;
        num_blockers        += is_blocking;
    }
    
    float penumbra_mask = num_blockers > 0.0 ? AvgBlockerDepthToPenumbra(light_size, total_blocker_depth / num_blockers, shadow_map_depth) : 0.0;
    float shadowness = 0.0;

    for (int i = 0; i < HYP_SHADOW_NUM_SAMPLES; i++) {
        vec2 filter_uv = VogelDisk(i, HYP_SHADOW_NUM_SAMPLES, gradient_noise);
        shadowness += GetShadow(index, pos, filter_uv * penumbra_mask * shadow_filter_size, NdotL);
    }

    shadowness /= float(HYP_SHADOW_NUM_SAMPLES);

    return shadowness;
}

/* Begin main shader program */

#define IBL_INTENSITY 2000.0
#define DIRECTIONAL_LIGHT_INTENSITY 200000.0
#define IRRADIANCE_MULTIPLIER 16.0
#define ROUGHNESS_LOD_MULTIPLIER 16.0
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
    vec4 albedo   = texture(gbuffer_albedo_texture, texcoord);
    vec4 normal   = vec4(DecodeNormal(texture(gbuffer_normals_texture, texcoord)), 1.0);
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
        ao = SampleEffectPre(0, v_texcoord0, vec4(1.0)).r * material.a;

        const float roughness = material.r;
        const float metalness = material.g;
        
        float NdotL = max(0.0001, dot(N, L));
        float NdotV = max(0.0001, dot(N, V));
        float NdotH = max(0.0001, dot(N, H));
        float LdotH = max(0.0001, dot(L, H));
        float VdotH = max(0.0001, dot(V, H));
        float HdotV = max(0.0001, dot(H, V));

        float shadow = 1.0;

#if HYP_SHADOW_PENUMBRA
        shadow = GetShadowContactHardened(0, position.xyz, NdotL);
#else
        shadow = GetShadow(0, position.xyz, vec2(0.0), NdotL);
#endif

        const vec3 diffuse_color = albedo_linear * (1.0 - metalness);

        const float material_reflectance = 0.5;
        const float reflectance = 0.16 * material_reflectance * material_reflectance; // dielectric reflectance
        const vec3 F0 = albedo_linear.rgb * metalness + (reflectance * (1.0 - metalness));
        
        const vec2 AB = BRDFMap(roughness, NdotV);
        const vec3 dfg = albedo_linear.rgb * AB.x + AB.y;
        
        const vec3 energy_compensation = 1.0 + F0 * (AB.y - 1.0);
        const float perceptual_roughness = sqrt(roughness);
        const float lod = ROUGHNESS_LOD_MULTIPLIER * perceptual_roughness * (2.0 - perceptual_roughness);
        
        vec3 irradiance = vec3(0.0);
        vec4 reflections = vec4(0.0);

        vec3 ibl = HasEnvironmentTexture(0)
            ? textureLod(cubemap_textures[scene.environment_texture_index], R, lod).rgb
            : vec3(0.0);
        
#if HYP_VCT_ENABLED
        vec4 vct_specular = ConeTraceSpecular(position.xyz, N, R, roughness);
        vec4 vct_diffuse  = ConeTraceDiffuse(position.xyz, N, vec3(0.0), vec3(0.0), roughness);

        irradiance  = vct_diffuse.rgb;
        reflections = vct_specular;
#endif

        vec3 light_color = vec3(1.0);
        
        // ibl
        //vec3 dfg =  AB;// mix(vec3(AB.x), vec3(AB.y), vec3(1.0) - F0);
        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        vec3 radiance = dfg * ibl * specular_ao;
        result = (diffuse_color * (irradiance * IRRADIANCE_MULTIPLIER) * (1.0 - dfg) * ao) + radiance;
        result *= exposure * IBL_INTENSITY;

        result = (result * (1.0 - reflections.a)) + (dfg * reflections.rgb);

        // surface
        vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));
        float D = DistributionGGX(N, H, roughness);
        float G = CookTorranceG(NdotL, NdotV, LdotH, NdotH);
        vec3 F = SchlickFresnel(F0, F90, LdotH);
        vec3 specular = D * G * F;
        //vec3 fresnel = (F * G * D) / (4.0 * NdotL * NdotV);
        
        vec3 light_scatter = SchlickFresnel(vec3(1.0), F90, NdotL);
        vec3 view_scatter  = SchlickFresnel(vec3(1.0), F90, NdotV);
        vec3 diffuse = diffuse_color * (light_scatter * view_scatter * (1.0 / PI));

        // Chan 2018, "Material Advances in Call of Duty: WWII"
        //float micro_shadow_aperture = inversesqrt(1.0 - ao);
        //float micro_shadow = clamp(NdotL * micro_shadow_aperture, 0.0, 1.0);
        //float micro_shadow_sqr = micro_shadow * micro_shadow;

        result += (specular + diffuse * energy_compensation) * ((exposure * DIRECTIONAL_LIGHT_INTENSITY) * NdotL * ao * shadow);
    
    } else {
        result = albedo.rgb;
    }

#if SSAO_DEBUG
    result = vec3(ao);
#endif

    output_color     = vec4(Tonemap(result), 1.0);
    //output_color     = texture(shadow_maps[0], texcoord);
    output_normals   = EncodeNormal(normal.xyz);
    output_positions = position;
}