#ifndef HYP_DEFERRED_LIGHTING_GLSL
#define HYP_DEFERRED_LIGHTING_GLSL

#include "../include/shared.inc"

struct Refraction
{
    vec3 position;
    vec3 direction;
};

float ApplyIORToRoughness(float ior, float roughness)
{
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
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