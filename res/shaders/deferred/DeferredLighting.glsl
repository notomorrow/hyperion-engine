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
#define ENV_GRID_MULTIPLIER 3.1415

struct DeferredParams
{
    uint flags;
    uint screen_width;
    uint screen_height;
};

struct Refraction
{
    vec3 position;
    vec3 direction;
};

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

// Compute attenuated light as it travels through a volume.
vec3 ApplyVolumeAttenuation(vec3 radiance, float transmission_distance, vec3 attenuation_color, float attenuation_distance)
{
    if (attenuation_distance == 0.0)
    {
        // Attenuation distance is +∞ (which we indicate by zero), i.e. the transmitted color is not attenuated at all.
        return radiance;
    }
    else
    {
        // Compute light attenuation using Beer's law.
        vec3 attenuation_coefficient = -log(attenuation_color) / attenuation_distance;
        vec3 transmittance = exp(-attenuation_coefficient * transmission_distance); // Beer's law
        return transmittance * radiance;
    }
}

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

    vec4 refraction_pos = camera.projection * camera.view * vec4(refraction.position, 1.0);
    refraction_pos /= refraction_pos.w;

    vec2 refraction_texcoord = refraction_pos.xy * 0.5 + 0.5;

    const float lod = ApplyIORToRoughness(IOR, roughness) * log2(float(max_dimension));

    float absorption = 0.1; // TODO: material parameter
    vec3 T = min(vec3(1.0), exp(-absorption * refraction.direction));

    vec3 Ft = Texture2DLod(sampler_linear, gbuffer_mip_chain, refraction_texcoord, lod).rgb;
    Ft *= translucent_color.rgb;
    Ft *= 1.0 - E;
    Ft *= T;

    return Ft;
}

#endif


#ifndef HYP_DEFERRED_NO_ENV_PROBE

#include "../include/env_probe.inc"

#ifndef HYP_DEFERRED_NO_ENV_GRID

float[9] ProjectSHBands(vec3 N)
{
    float bands[9];

    bands[0] = 0.282095;
    bands[1] = -0.488603 * N.y;
    bands[2] = 0.488603 * N.z;
    bands[3] = -0.488603 * N.x;
    bands[4] = 1.092548 * N.x * N.y;
    bands[5] = -1.092548 * N.y * N.z;
    bands[6] = 0.315392 * (3.0 * N.z * N.z - 1.0);
    bands[7] = -1.092548 * N.x * N.z;
    bands[8] = 0.546274 * (N.x * N.x - N.y * N.y);

    return bands;
}

vec3 CalculateEnvProbeIrradiance(vec3 P, vec3 N)
{
    int base_probe_index_at_point = int(GetLocalEnvProbeIndex(env_grid, P));

    if (base_probe_index_at_point < 0 || base_probe_index_at_point >= HYP_MAX_BOUND_AMBIENT_PROBES) {
        return vec3(0.0);
    }

    const uint base_probe_index = GET_GRID_PROBE_INDEX(base_probe_index_at_point);

    if (base_probe_index == ~0u) {
        return vec3(0.0);
    }

    const vec3 size_of_probe = env_grid.aabb_extent.xyz / vec3(env_grid.density.xyz);

    const vec3 base_probe_position = env_probes[base_probe_index].world_position.xyz;

    const vec3 alpha = clamp((P - base_probe_position) / size_of_probe, vec3(0.0), vec3(1.0)); //clamp(abs((P - base_probe_position) / size_of_probe * 0.5), vec3(0.0), vec3(1.0));
    // const vec3 alpha = vec3(1.0) - clamp(distance(P, base_probe_position) / size_of_probe * 0.5, vec3(0.0), vec3(1.0)); //clamp(abs((P - base_probe_position) / size_of_probe * 0.5), vec3(0.0), vec3(1.0));
    
    SH9 sh9;

    vec3 total_irradiance = vec3(0.0);
    float total_weight = 0.0;

     // iterate over probe cage
    for (int i = 0; i < 8; i++) {
        ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);

        // Add (size_of_probe * 0.5) to bias it to ensure the correct probe is selected
        vec3 neighbor_probe_position = base_probe_position + (size_of_probe * vec3(offset)) + (size_of_probe * 0.5);

        int neighbor_probe_index_at_point = int(GetLocalEnvProbeIndex(env_grid, neighbor_probe_position));

        if (neighbor_probe_index_at_point < 0 || neighbor_probe_index_at_point >= HYP_MAX_BOUND_AMBIENT_PROBES) {
            continue;
        }

        const uint neighbor_probe_index = GET_GRID_PROBE_INDEX(neighbor_probe_index_at_point);

        if (neighbor_probe_index == ~0u) {
            continue;
        }

        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight = trilinear.x * trilinear.y * trilinear.z;

        const int storage_index = env_probes[neighbor_probe_index].position_in_grid.w * 9;

        for (int j = 0; j < 9; j++) {
            sh9.values[j] = sh_grid_buffer[min(storage_index + j, SH_GRID_BUFFER_SIZE - 1)].rgb;
        }

        total_irradiance += SphericalHarmonicsSample(sh9, N) * weight;
        total_weight += weight;
    }

    // const uint probe_index = GET_GRID_PROBE_INDEX(probe_index_at_point);



    // if (probe_index != ~0u) {
    //     EnvProbe probe = env_probes[probe_index];
    //     const int storage_index = probe.position_in_grid.w * 9;

    //     for (int i = 0; i < 9; i++) {
    //         sh9.values[i] = sh_grid_buffer[min(storage_index + i, SH_GRID_BUFFER_SIZE - 1)].rgb;
    //     }

    //     irradiance += SphericalHarmonicsSample(sh9, N);

    //     return 1.0;
    // }

    const vec3 debug_colors[16] = {
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0),
        vec3(1.0, 1.0, 0.0),
        vec3(1.0, 0.0, 1.0),
        vec3(0.0, 1.0, 1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(0.5, 0.0, 0.0),
        vec3(0.0, 0.5, 0.0),
        vec3(0.0, 0.0, 0.5),
        vec3(0.5, 0.5, 0.0),
        vec3(0.5, 0.0, 0.5),
        vec3(0.0, 0.5, 0.5),
        vec3(0.5, 0.5, 0.5),
        vec3(0.25, 0.0, 0.0),
        vec3(0.0, 0.25, 0.0)
    };

    // return debug_colors[base_probe_index % 16];

    // return UINT_TO_VEC4(base_probe_index).rgb;

    return total_irradiance / max(total_weight, 0.0001);
}

#endif

void ApplyReflectionProbe(in EnvProbe probe, vec3 P, vec3 R, float lod, inout vec4 ibl)
{
    ibl = vec4(0.0);

    if (probe.texture_index == ~0u) {
        return;
    }
    
    const uint probe_texture_index = max(0, min(probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));

    const vec3 extent = (probe.aabb_max.xyz - probe.aabb_min.xyz);
    const vec3 center = (probe.aabb_max.xyz + probe.aabb_min.xyz) * 0.5;
    const vec3 diff = P - center;

#ifdef ENV_PROBE_PARALLAX_CORRECTED
    R = EnvProbeCoordParallaxCorrected(probe, P, R);
#endif

    ibl = EnvProbeSample(
        sampler_linear,
        env_probe_textures[probe_texture_index],
        R,
        lod
    );
}

vec4 CalculateReflectionProbe(in EnvProbe probe, vec3 P, vec3 N, vec3 R, vec3 camera_position, float roughness)
{
    vec4 ibl = vec4(0.0);

    const float lod = float(8.0) * roughness * (2.0 - roughness);

#ifndef ENV_PROBE_PARALLAX_CORRECTED
    // ENV_PROBE_PARALLAX_CORRECTED is not statically defined, we need to use flags on the EnvProbe struct
    // at render time to determine if the probe is parallax corrected.
    const bool is_parallax_corrected = bool(probe.flags & HYP_ENV_PROBE_PARALLAX_CORRECTED);

    if (is_parallax_corrected) {
        R = EnvProbeCoordParallaxCorrected(probe, P, R);
    }
#endif

    ApplyReflectionProbe(probe, P, R, lod, ibl);

    return ibl;
}

#endif

#ifndef HYP_DEFERRED_NO_SSR
#ifdef SSR_ENABLED
void CalculateScreenSpaceReflection(DeferredParams deferred_params, vec2 uv, float depth, inout vec3 reflections)
{
    const bool enabled = bool(deferred_params.flags & DEFERRED_FLAGS_SSR_ENABLED);

    // if (!enabled || depth > 0.999) {
    //     return;
    // }

    vec4 screen_space_reflections = Texture2D(sampler_linear, ssr_result, uv);
    screen_space_reflections.a = saturate(screen_space_reflections.a * float(enabled));
    // screen_space_reflections.rgb = pow(screen_space_reflections.rgb, vec3(2.2));
    // screen_space_reflections.rgb = ReverseTonemapReinhardSimple(screen_space_reflections.rgb);
    reflections = mix(reflections, screen_space_reflections.rgb, screen_space_reflections.a);
}
#endif
#endif

#ifndef HYP_DEFERRED_NO_RT_RADIANCE
#ifdef PATHTRACER
vec4 CalculatePathTracing(DeferredParams deferred_params, vec2 uv)
{
    return Texture2DLod(sampler_linear, rt_radiance_final, uv, 0.0);
}
#endif

#ifdef RT_REFLECTIONS_ENABLED
void CalculateRaytracingReflection(DeferredParams deferred_params, vec2 uv, inout vec3 reflections)
{
    const bool enabled = bool(deferred_params.flags & DEFERRED_FLAGS_RT_RADIANCE_ENABLED);

    vec4 rt_radiance = Texture2DLod(sampler_linear, rt_radiance_final, uv, 0.0);
    rt_radiance *= float(enabled);

    reflections = mix(reflections, rt_radiance.rgb, rt_radiance.a);
}
#endif
#endif

void CalculateHBILIrradiance(DeferredParams deferred_params, in vec4 ssao_data, inout vec3 irradiance)
{
    irradiance += pow(ssao_data.rgb, vec3(2.2)) * HYP_HBIL_MULTIPLIER * float(bool(deferred_params.flags & DEFERRED_FLAGS_HBIL_ENABLED));
}

void IntegrateReflections(inout vec3 Fr, in vec4 reflections)
{
    Fr = (Fr * (1.0 - reflections.a)) + (reflections.rgb);
}

void ApplyFog(in vec3 P, inout vec4 result)
{
    result = CalculateFogLinear(result, unpackUnorm4x8(uint(scene.fog_params.x)), P, camera.position.xyz, scene.fog_params.y, scene.fog_params.z);
}

#endif