#include "include/Defines.hlsli"
#include "include/Shared.hlsli"
#include "include/Scene.hlsli"

DECLARE_SRV(TAA, InColorTexture) Texture2D color_texture;
DECLARE_SRV(TAA, InPrevColorTexture) Texture2D prev_color_texture;
DECLARE_SRV(TAA, InVelocityTexture) Texture2D velocity_texture;
DECLARE_SRV(TAA, InDepthTexture) Texture2D depth_texture;
DECLARE_SRV(TAA, InMaterialTexture) Texture2D<uint> material_texture;

DECLARE_SAMPLER(TAA, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(TAA, SamplerNearest) SamplerState sampler_nearest;

DECLARE_UAV(TAA, OutColorImage) RWTexture2D<float4> output_image;

DECLARE_BUFFER_DYNAMIC(TAA, TAAConstants) cbuffer TAAConstants
{
    uint4 dimensions; // zw = depth
    float4 jitter;
    float feedback; // Rendering.TAA.Feedback
    float cutoutFeedback; // Rendering.TAA.CutoutFeedback
};

#define FEEDBACK feedback

#define CUTOUT_VELOCITY_REJECTION_PIXELS 16.0

#define TEMPORAL_BLENDING_REVERSE_TONEMAP

#include "include/Temporal.hlsli"

#define TAA_MAX_RADIANCE 16384.0
#define VARIANCE_CLIP_GAMMA 1.0
#define CUTOUT_VARIANCE_CLIP_GAMMA 1.25

float4 SanitizeColor(float4 color)
{
    const uint3 bits = asuint(color.rgb);
    const uint3 magnitude = bits & 0x7FFFFFFFu;

    const bool3 is_nan_or_negative = or(magnitude > 0x7F800000u, (bits >> 31u) != 0u);
    const bool3 is_large = magnitude >= asuint(TAA_MAX_RADIANCE);

    return float4(select(is_nan_or_negative, (float3)0.0, select(is_large, (float3)TAA_MAX_RADIANCE, color.rgb)), 1.0);
}

float4 ResolveTAA(int2 pixel_coord, float2 uv, float2 velocity, float2 texel_size, bool is_cutout)
{
    const int2 color_dimensions = int2(dimensions.xy);

    // content at unjittered pixel p was rendered at p + jitter_px (ndc y up, pixels y down)
    const float2 jitter_px = float2(jitter.x, -jitter.y) * 0.5 * float2(color_dimensions);

    float4 moment1 = (float4)0.0;
    float4 moment2 = (float4)0.0;
    float4 box_min = (float4)1e5;
    float4 box_max = (float4)-1e5;
    float3 neighborhood_max_rgb = (float3)0.0;

    float4 current = (float4)0.0;
    float current_weight_sum = 0.0;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            const int2 coord = clamp(pixel_coord + int2(x, y), int2(0, 0), color_dimensions - 1);

            const float4 neighbor_sample = SanitizeColor(color_texture.Load(int3(coord, 0)));
            const float4 neighbor_rgb = ADJUST_COLOR_GAMMA_IN(neighbor_sample);
            const float4 neighbor = RGBToYCoCg(neighbor_rgb);

            const float2 offset = float2(x, y) - jitter_px;
            const float weight = exp(-2.29 * dot(offset, offset));

            current += neighbor * weight;
            current_weight_sum += weight;

            moment1 += neighbor;
            moment2 += neighbor * neighbor;
            box_min = min(box_min, neighbor);
            box_max = max(box_max, neighbor);
            neighborhood_max_rgb = max(neighborhood_max_rgb, neighbor_rgb.rgb);
        }
    }

    current /= current_weight_sum;

    const float2 history_uv = uv - velocity;

    const bool is_velocity_valid = !any(HYP_ISNAN(velocity)) && !any(HYP_ISINF(velocity));

    if (!is_velocity_valid || any(history_uv < 0.0) || any(history_uv > 1.0))
    {
        const float4 current_rgb = YCoCgToRGB(current);
        return ADJUST_COLOR_GAMMA_OUT(current_rgb);
    }

    const float4 history_sample = float4(SampleTextureCatmullRom(prev_color_texture, history_uv, texel_size).rgb, 1.0);
    const float4 history = RGBToYCoCg(ADJUST_COLOR_GAMMA_IN(history_sample));

    const float gamma = is_cutout ? CUTOUT_VARIANCE_CLIP_GAMMA : VARIANCE_CLIP_GAMMA;
    const float4 mean = moment1 / 9.0;
    const float4 sigma = sqrt(max(moment2 / 9.0 - mean * mean, 0.0));

    const float4 clip_min = max(box_min, mean - sigma * gamma);
    const float4 clip_max = min(box_max, mean + sigma * gamma);

    const float4 clipped_history = ClipAABB(clip_min, clip_max, clamp(current, clip_min, clip_max), history);

    float4 resolved;

    if (is_cutout)
    {
        resolved = lerp(current, clipped_history, cutoutFeedback);

        const float pixel_velocity = length(velocity / texel_size);
        resolved = lerp(resolved, current, saturate(pixel_velocity / CUTOUT_VELOCITY_REJECTION_PIXELS));
    }
    else
    {
        resolved = TemporalLuminanceResolveYCoCg(current, clipped_history, FEEDBACK);
    }

    float4 resolved_rgb = YCoCgToRGB(resolved);

    // the YCoCg box reaches past the neighborhood's rgb range, and anything near 1 explodes once the HDR tonemap is inverted
    resolved_rgb.rgb = clamp(resolved_rgb.rgb, 0.0, neighborhood_max_rgb);

    resolved_rgb = ADJUST_COLOR_GAMMA_OUT(resolved_rgb);

    return float4(min(resolved_rgb.rgb, TAA_MAX_RADIANCE), 1.0);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= dimensions.x || dispatchThreadID.y >= dimensions.y)
    {
        return;
    }

    const uint2 colorDimensions = dimensions.xy;
    const uint2 depthDimensions = dimensions.zw;

    const int2 pixel_coord = int2(dispatchThreadID.xy);

    const float2 uv = (float2(pixel_coord) + 0.5) / float2(colorDimensions);

    const float2 texel_size = float2(1.0, 1.0) / float2(colorDimensions);
    const float2 depth_texel_size = float2(1.0, 1.0) / float2(depthDimensions);

    const float3 closest_fragment = ClosestFragment(depth_texture, uv, depth_texel_size);
    const float2 velocity = SAMPLE_TEXTURE_2D(sampler_nearest, velocity_texture, closest_fragment.xy).rg;

    // the closest fragment is the nearest surface around the pixel, so dithered leaves still count where this pixel was discarded
    const uint2 closestTexel = min(uint2(closest_fragment.xy * float2(depthDimensions)), depthDimensions - 1);
    const bool isCutout = ((material_texture.Load(int3(closestTexel, 0)) >> 28u) & OBJECT_MASK_CUTOUT) != 0;

    const float4 result = ResolveTAA(pixel_coord, uv, velocity, texel_size, isCutout);

    // cheeky nan check to prevent poisoning
    output_image[pixel_coord] = any(HYP_ISNAN(result)) ? float4(0.0, 1.0, 0.0, 1.0) : result;
}
