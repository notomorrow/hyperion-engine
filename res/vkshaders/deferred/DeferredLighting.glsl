#ifndef HYP_DEFERRED_LIGHTING_GLSL
#define HYP_DEFERRED_LIGHTING_GLSL

#include "../include/shared.inc"
#include "../include/brdf.inc"

#define DEFERRED_FLAGS_SSR_ENABLED 0x1
#define DEFERRED_FLAGS_VCT_ENABLED 0x2
#define DEFERRED_FLAGS_ENV_PROBE_ENABLED 0x4
#define DEFERRED_FLAGS_HBAO_ENABLED 0x8
#define DEFERRED_FLAGS_HBIL_ENABLED 0x10
#define DEFERRED_FLAGS_RT_RADIANCE_ENABLED 0x20

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

    Refraction refraction;
    refraction.position = vec3(P + R * d);
    vec3 n1 = normalize(NdotR * R - N * 0.5);
    refraction.direction = refract(R, n1, eta_ir);

    out_refraction = refraction;
}

#ifndef HYP_DEFERRED_NO_REFRACTION

vec3 CalculateRefraction(
    vec3 P, vec3 N, vec3 V, vec2 texcoord,
    vec3 F0, vec3 E,
    float transmission, float roughness,
    vec4 opaque_color, vec4 translucent_color
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

    vec3 Fd = translucent_color.rgb * (1.0 /*irradiance*/) * (1.0 - E) * (1.0 /* diffuse BRDF */);
    Fd *= (1.0 - transmission);

    vec3 Ft = Texture2DLod(sampler_linear, gbuffer_mip_chain, refraction_texcoord, lod).rgb;

    Ft *= translucent_color.rgb;
    Ft *= 1.0 - E;
    // Ft *= absorption;
    Ft *= transmission;

    return Ft;
}

#endif

// #ifdef ENV_PROBE_ENABLED

#include "../include/env_probe.inc"

// gives not-so accurate indirect light for the scene --
// sample lowest mipmap of cubemap, if we have it.
// later, replace this will spherical harmonics.
void CalculateEnvProbeIrradiance(DeferredParams deferred_params, vec3 N, inout vec3 irradiance)
{
    if (scene.environment_texture_usage != 0) {
        const uint probe_index = scene.environment_texture_index;
        EnvProbe probe = env_probes[probe_index];

        if (probe.texture_index != ~0u) {
            const uint probe_texture_index = max(0, min(probe.texture_index, HYP_MAX_BOUND_ENV_PROBES));
            const int num_levels = GetNumLevels(sampler_linear, env_probe_textures[probe_texture_index]);

            vec4 env_probe_irradiance = EnvProbeSample(sampler_linear, env_probe_textures[probe_texture_index], N, float(num_levels - 1));

            irradiance += env_probe_irradiance.rgb * env_probe_irradiance.a;
        }
    }
}

vec3 CalculateEnvProbeReflection(DeferredParams deferred_params, vec3 P, vec3 N, vec3 R, float perceptual_roughness)
{
    vec3 ibl = vec3(0.0);

    if (scene.environment_texture_usage != 0) {
        const uint probe_index = scene.environment_texture_index;
        EnvProbe probe = env_probes[probe_index];

        if (probe.texture_index != ~0u) {
            const uint probe_texture_index = max(0, min(probe.texture_index, HYP_MAX_BOUND_ENV_PROBES));
            const int num_levels = GetNumLevels(sampler_linear, env_probe_textures[probe_texture_index]);
            const float lod = float(num_levels) * perceptual_roughness * (2.0 - perceptual_roughness);

            ibl = EnvProbeSample(
                gbuffer_sampler,
                env_probe_textures[probe_texture_index],
                bool(probe.flags & HYP_ENV_PROBE_PARALLAX_CORRECTED)
                    ? EnvProbeCoordParallaxCorrected(probe, P, R)
                    : R,
                lod
            ).rgb;
        }
    }

    return ibl;
}

// #endif

#ifdef SSR_ENABLED
void CalculateScreenSpaceReflection(DeferredParams deferred_params, vec2 uv, inout vec4 reflections)
{
    const bool enabled = bool(deferred_params.flags & DEFERRED_FLAGS_SSR_ENABLED);

    vec4 screen_space_reflections = Texture2D(sampler_linear, ssr_blur_vert, uv);
    screen_space_reflections.rgb = pow(screen_space_reflections.rgb, vec3(2.2));
    reflections = mix(reflections, screen_space_reflections, screen_space_reflections.a * float(enabled));
}
#endif

#ifdef RT_ENABLED
void CalculateRaytracingReflection(DeferredParams deferred_params, vec2 uv, inout vec4 reflections)
{
    const bool enabled = bool(deferred_params.flags & DEFERRED_FLAGS_RT_RADIANCE_ENABLED);

    vec4 rt_radiance = Texture2DLod(sampler_linear, rt_radiance_final, uv, 0.0);
    reflections = mix(reflections, rt_radiance, rt_radiance.a * float(enabled));
}
#endif

void CalculateHBILIrradiance(DeferredParams deferred_params, in vec4 ssao_data, inout vec3 irradiance)
{
    irradiance += ssao_data.rgb * float(bool(deferred_params.flags & DEFERRED_FLAGS_HBIL_ENABLED));
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