#ifndef HYP_DEFERRED_LIGHTING_GLSL
#define HYP_DEFERRED_LIGHTING_GLSL

#include "../include/shared.inc"
#include "../include/brdf.inc"
#include "../include/tonemap.inc"

#define DEFERRED_FLAGS_SSR_ENABLED         0x1
#define DEFERRED_FLAGS_VCT_ENABLED         0x2
#define DEFERRED_FLAGS_ENV_PROBE_ENABLED   0x4
#define DEFERRED_FLAGS_HBAO_ENABLED        0x8
#define DEFERRED_FLAGS_HBIL_ENABLED        0x10
#define DEFERRED_FLAGS_RT_RADIANCE_ENABLED 0x20

#define HYP_HBIL_MULTIPLIER 1.0
#define ENV_PROBE_MULTIPLIER 3.0

struct DeferredParams
{
    uint flags;
};

struct Refraction
{
    vec3 position;
    vec3 direction;
};

float ApplyIORToRoughness(float ior, float roughness)
{
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}

vec3 CalculateDiffuseColor(vec3 albedo, float metalness)
{
    return albedo * (1.0 - metalness);
}

vec4 CalculateDiffuseColor(vec4 albedo, float metalness)
{
    return vec4(CalculateDiffuseColor(albedo.rgb, metalness), albedo.a);
}

vec3 CalculateF0(vec3 albedo, float metalness)
{
    const float material_reflectance = 0.5;
    const float reflectance = 0.16 * material_reflectance * material_reflectance;
    return albedo * metalness + (reflectance * (1.0 - metalness));
}

vec3 CalculateFresnelTerm(vec3 F0, float roughness, float NdotV)
{
    return SchlickFresnelRoughness(F0, roughness, NdotV);
}

vec4 CalculateFresnelTerm(vec4 F0, float roughness, float NdotV)
{
    return SchlickFresnelRoughness(F0, roughness, NdotV);
}

float CalculateGeometryTerm(float NdotL, float NdotV, float HdotV, float NdotH)
{
    return CookTorranceG(NdotL, NdotV, HdotV, NdotH);
}

float CalculateDistributionTerm(float roughness, float NdotH)
{
    return Trowbridge(NdotH, roughness);
}

vec3 CalculateDFG(vec3 F, float perceptual_roughness, float NdotV)
{
    const vec2 AB = BRDFMap(perceptual_roughness, NdotV);

    return F * AB.x + AB.y;
}

vec4 CalculateDFG(vec4 F, float perceptual_roughness, float NdotV)
{
    const vec2 AB = BRDFMap(perceptual_roughness, NdotV);

    return F * AB.x + AB.y;
}

vec3 CalculateE(vec3 F0, vec3 dfg)
{
    return mix(dfg.xxx, dfg.yyy, F0);
}

vec4 CalculateE(vec4 F0, vec4 dfg)
{
    return mix(dfg.xxxx, dfg.yyyy, F0);
}

vec3 CalculateEnergyCompensation(vec3 F0, vec3 dfg)
{
    return 1.0 + F0 * ((1.0 / max(dfg.y, 0.0001)) - 1.0);
}

vec4 CalculateEnergyCompensation(vec4 F0, vec4 dfg)
{
    return 1.0 + F0 * ((1.0 / max(dfg.y, 0.0001)) - 1.0);
}

void RefractionSolidSphere(
    vec3 P, vec3 N, vec3 V, float eta_ir,
    out Refraction out_refraction
)
{
    const float thickness = 0.8;

    const vec3 R = refract(-V, N, eta_ir);
    float NdotR = dot(N, R);
    float d = thickness * -NdotR;
    vec3 n1 = normalize(NdotR * R - N * 0.5);

    Refraction refraction;
    refraction.position = vec3(P + R * d);
    refraction.direction = refract(R, n1, eta_ir);

    out_refraction = refraction;
}

#ifndef HYP_DEFERRED_NO_REFRACTION

vec3 CalculateRefraction(
    uvec2 image_dimensions,
    vec3 P, vec3 N, vec3 V, vec2 texcoord,
    vec3 F0, vec3 E,
    float transmission, float roughness,
    vec4 opaque_color, vec4 translucent_color,
    vec3 brdf
)
{
    // dimensions of mip chain image
    const uint max_dimension = max(image_dimensions.x, image_dimensions.y);

    const float IOR = 1.5;

    const float sqrt_F0 = sqrt(F0.y);
    const float material_ior = (1.0 + sqrt_F0) / (1.0 - sqrt_F0);
    const float air_ior = 1.0;
    const float eta_ir = air_ior / material_ior;
    const float eta_ri = material_ior / air_ior;

    Refraction refraction;
    RefractionSolidSphere(P, N, V, eta_ir, refraction);

    vec4 refraction_pos = scene.projection * scene.view * vec4(refraction.position, 1.0);
    refraction_pos /= refraction_pos.w;

    vec2 refraction_texcoord = refraction_pos.xy * 0.5 + 0.5;

    // float perceptual_roughness = sqrt(roughness);
    // perceptual_roughness = mix(perceptual_roughness, 0.0, Saturate(eta_ir * 3.0 - 2.0));

    const float lod = ApplyIORToRoughness(IOR, roughness) * log2(float(max_dimension));

    vec3 Fd = translucent_color.rgb * (1.0 /*irradiance*/) * (1.0 - E) * (brdf);
    Fd *= (1.0 - transmission);

    const float texel_size = 1.0 / float(max_dimension);

    vec3 Ft = Texture2DLod(sampler_linear, gbuffer_mip_chain, refraction_texcoord, lod).rgb;

    Ft *= translucent_color.rgb;
    Ft *= 1.0 - E;
    // Ft *= absorption;
    Ft *= transmission;

    return Ft;
}

#endif


#ifndef HYP_DEFERRED_NO_ENV_PROBE
#ifdef ENV_PROBE_ENABLED

#include "../include/env_probe.inc"

int GetLocalEnvProbeIndex(vec3 world_position, out ivec3 unit_diff)
{
    const vec3 diff = world_position - env_grid.center.xyz;
    const vec3 pos_clamped = (diff / env_grid.aabb_extent.xyz) + 0.5;

    if (any(greaterThanEqual(pos_clamped, vec3(1.0))) || any(lessThan(pos_clamped, vec3(0.0)))) {
        unit_diff = ivec3(0);
        return -1;
    }

    unit_diff = ivec3(pos_clamped * vec3(env_grid.density.xyz));

    int probe_index_at_point = (int(unit_diff.x) * int(env_grid.density.y) * int(env_grid.density.z))
        + (int(unit_diff.y) * int(env_grid.density.z))
        + int(unit_diff.z) + 1 /* + 1 because the first element is always the reflection probe */;

    return probe_index_at_point;
}

int GetLocalEnvProbeIndexOffset(vec3 world_position, vec3 offset)
{
    vec3 diff = world_position - env_grid.center.xyz;
    vec3 pos_clamped = (diff / env_grid.aabb_extent.xyz) + 0.5;

    if (any(greaterThanEqual(pos_clamped, vec3(1.0))) || any(lessThan(pos_clamped, vec3(0.0)))) {
        return -1;
    }

    ivec3 unit_diff = ivec3(pos_clamped * vec3(env_grid.density.xyz)) + ivec3(floor(offset));

    int offset_index = (int(unit_diff.x) * int(env_grid.density.y) * int(env_grid.density.z))
        + (int(unit_diff.y) * int(env_grid.density.z))
        + int(unit_diff.z) + 1 /* + 1 because the first element is always the reflection probe */;

    return offset_index;
}

void _ApplyEnvProbeSample(uint probe_index, vec3 P, vec3 R, float lod, inout vec3 ibl, bool include_shadow)
{
    EnvProbe probe = GET_GRID_PROBE(probe_index);

    if (probe.texture_index != ~0u) {
        const uint probe_texture_index = max(0, min(probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));

        const vec3 extent = (probe.aabb_max.xyz - probe.aabb_min.xyz);
        const vec3 center = (probe.aabb_max.xyz + probe.aabb_min.xyz) * 0.5;
        const vec3 diff = P - center;

        const bool is_parallax_corrected = bool(probe.flags & HYP_ENV_PROBE_PARALLAX_CORRECTED);

        vec4 env_probe_sample = EnvProbeSample(
            sampler_linear,
            env_probe_textures[probe_texture_index],
            is_parallax_corrected
                ? EnvProbeCoordParallaxCorrected(probe, P, R)
                : R,
            lod
        );

        env_probe_sample.rgb = ReverseTonemapReinhardSimple(env_probe_sample.rgb);

        ibl = env_probe_sample.rgb * mix(1.0, max(0.1, env_probe_sample.a), float(include_shadow));
    }
}

// gives not-so accurate indirect light for the scene --
// sample lowest mipmap of cubemap, if we have it.
// later, replace this will spherical harmonics.
void CalculateEnvProbeIrradiance(DeferredParams deferred_params, vec3 P, vec3 N, inout vec3 irradiance)
{
    ivec3 probe_position;
    int probe_index_at_point = int(GetLocalEnvProbeIndex(P, probe_position));

    if (probe_index_at_point < 1 || probe_index_at_point >= HYP_MAX_BOUND_AMBIENT_PROBES) {
        return;
    }

    const float lod = 0.0;

    if (bool(env_grid.enabled_indices_mask & (1u << uint(probe_index_at_point)))) {
        const ivec3 image_size = textureSize(sampler3D(spherical_harmonics_volumes[0], sampler_linear), 0);
        const vec3 texel_size = vec3(1.0) / vec3(image_size);

        EnvProbe probe = GET_GRID_PROBE(probe_index_at_point);
        const vec3 extent = (probe.aabb_max.xyz - probe.aabb_min.xyz);
        const vec3 extent_unpadded = env_grid.aabb_extent.xyz / vec3(env_grid.density.xyz);
        const vec3 center = (probe.aabb_max.xyz + probe.aabb_min.xyz) * 0.5;

        const vec3 pos_relative_to_grid = P - env_grid.center.xyz;
        const vec3 pos_relative_to_probe = ((P - center) / (extent_unpadded)) + 0.5;

        vec3 coord = (vec3(probe_position) + 0.5) * texel_size;
        coord += vec3(pos_relative_to_probe - 0.5) * texel_size;

        SH9 sh;
        sh.values[0] = Texture3D(sampler_linear, spherical_harmonics_volumes[0], coord);
        sh.values[1] = Texture3D(sampler_linear, spherical_harmonics_volumes[1], coord);
        sh.values[2] = Texture3D(sampler_linear, spherical_harmonics_volumes[2], coord);
        sh.values[3] = Texture3D(sampler_linear, spherical_harmonics_volumes[3], coord);
        sh.values[4] = Texture3D(sampler_linear, spherical_harmonics_volumes[4], coord);
        sh.values[5] = Texture3D(sampler_linear, spherical_harmonics_volumes[5], coord);
        sh.values[6] = Texture3D(sampler_linear, spherical_harmonics_volumes[6], coord);
        sh.values[7] = Texture3D(sampler_linear, spherical_harmonics_volumes[7], coord);
        sh.values[8] = Texture3D(sampler_linear, spherical_harmonics_volumes[8], coord);

        irradiance += SphericalHarmonicsSample(sh, N) * ENV_PROBE_MULTIPLIER;
    }
}

vec3 CalculateEnvProbeReflection(DeferredParams deferred_params, vec3 P, vec3 N, vec3 R, vec3 camera_position, float perceptual_roughness)
{
    vec3 ibl = vec3(0.0);

    const float lod = float(9.0) * perceptual_roughness * (2.0 - perceptual_roughness);

    if (bool(env_grid.enabled_indices_mask & (1 << 0))) {
        _ApplyEnvProbeSample(0, P, R, lod, ibl, true);
    }

    return ibl;// * ENV_PROBE_MULTIPLIER;
}

#endif
#endif

#ifndef HYP_DEFERRED_NO_SSR
#ifdef SSR_ENABLED
void CalculateScreenSpaceReflection(DeferredParams deferred_params, vec2 uv, float depth, inout vec4 reflections)
{
    const bool enabled = bool(deferred_params.flags & DEFERRED_FLAGS_SSR_ENABLED);

    vec4 screen_space_reflections = Texture2D(sampler_linear, ssr_result, uv);
    screen_space_reflections.rgb = pow(screen_space_reflections.rgb, vec3(2.2));
    screen_space_reflections.rgb = ReverseTonemapReinhardSimple(screen_space_reflections.rgb);
    reflections = mix(reflections, screen_space_reflections, screen_space_reflections.a * float(enabled)/* * float(depth < 0.995)*/);
}
#endif
#endif

#ifndef HYP_DEFERRED_NO_RT_RADIANCE
#ifdef RT_ENABLED
void CalculateRaytracingReflection(DeferredParams deferred_params, vec2 uv, inout vec4 reflections)
{
    const bool enabled = bool(deferred_params.flags & DEFERRED_FLAGS_RT_RADIANCE_ENABLED);

    vec4 rt_radiance = Texture2DLod(sampler_linear, rt_radiance_final, uv, 0.0);
    rt_radiance.rgb = pow(rt_radiance.rgb, vec3(2.2));
    reflections = mix(reflections, rt_radiance, rt_radiance.a * float(enabled));
}
#endif
#endif

void CalculateHBILIrradiance(DeferredParams deferred_params, in vec4 ssao_data, inout vec3 irradiance)
{
    irradiance += pow(ssao_data.rgb, vec3(2.2)) * HYP_HBIL_MULTIPLIER * float(bool(deferred_params.flags & DEFERRED_FLAGS_HBIL_ENABLED));
}

void IntegrateReflections(inout vec3 Fr, in vec3 E, in vec4 reflections)
{
    Fr = (Fr * (1.0 - reflections.a)) + (E * reflections.rgb);
}

void ApplyFog(in vec3 P, inout vec4 result)
{
    result = CalculateFogLinear(result, unpackUnorm4x8(uint(scene.fog_params.x)), P, scene.camera_position.xyz, scene.fog_params.y, scene.fog_params.z);
}

#endif