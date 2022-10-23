
#extension GL_GOOGLE_include_directive : require

#include "ssr_header.inc"
#include "../include/noise.inc"
#include "../include/shared.inc"

#define HYP_SSR_MAX_BLUR_INCREMENT 2.0
// #define HYP_SSR_USE_GAUSSIAN_BLUR 

#ifndef HYP_SSR_USE_GAUSSIAN_BLUR
    #define HYP_SSR_BLUR_MIN        0.25
    #define HYP_SSR_BLUR_MULTIPLIER 30.0
#endif

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 14, r8) uniform image2D ssr_radius_image;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 13, rgba8) uniform readonly image2D ssr_sample_image;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 18) uniform texture2D ssr_sample;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 19) uniform texture2D ssr_radius;

#if defined(HYP_SSR_BLUR_HORIZONTAL)
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 15, rgba8) uniform writeonly image2D ssr_blur;
    #define HYP_SSR_PREV_IMAGE   ssr_sample_image
    #define HYP_SSR_PREV_TEXTURE ssr_sample
#elif defined(HYP_SSR_BLUR_VERTICAL)
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 20) uniform texture2D ssr_blur_hor;
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 15, rgba8) uniform readonly image2D ssr_blur_hor_image;
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 16, rgba8) uniform writeonly image2D ssr_blur;
    #define HYP_SSR_PREV_IMAGE   ssr_blur_hor_image
    #define HYP_SSR_PREV_TEXTURE ssr_blur_hor
#else
    #error No blur direction defined
#endif

#ifdef HYP_SSR_USE_GAUSSIAN_BLUR

void GaussianBlurAccum(
    vec2 texcoord,
    int direction,
    inout vec4 reflection_sample,
    inout float accum_radius,
    inout float divisor
)
{
    for (int i = 1; i < GAUSS_TABLE_SIZE; i++) {
        const float d = float(i) * HYP_SSR_MAX_BLUR_INCREMENT;

    #if defined(HYP_SSR_BLUR_HORIZONTAL)
        const ivec2 blur_step = ivec2(d, 0);
    #elif defined(HYP_SSR_BLUR_VERTICAL)
        const ivec2 blur_step = ivec2(0, d);
    #else
        #error No direction defined
    #endif


#ifdef HYP_SSR_BLUR_VERTICAL
        const float inv_aspect = float(ssr_params.dimension.y) / float(ssr_params.dimension.x);
#endif

        ivec2 tc = ivec2(texcoord * vec2(ssr_params.dimension)) + blur_step * direction;//ivec2(blur_step * direction);

#ifdef HYP_SSR_BLUR_VERTICAL
        //tc *= inv_aspect;
#endif

        float r = accum_radius;
        const float sample_radius = r * 255.0;

        if (d < sample_radius) {
            float weight = GaussianWeight(d / sample_radius);

            reflection_sample += imageLoad(HYP_SSR_PREV_IMAGE, tc) * weight;//Texture2D(gbuffer_sampler, HYP_SSR_PREV_IMAGE, tc) * weight;


            divisor += weight;
        }
    }
}

#endif

void main(void)
{
    const ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	const vec2 texcoord = vec2(coord) / vec2(ssr_params.dimension.x, ssr_params.dimension.y);

    vec4 reflection_sample = vec4(0.0);
    float accum_radius = 0.0;

#ifdef HYP_SSR_USE_GAUSSIAN_BLUR
    float divisor = gauss_table[0];

    reflection_sample = imageLoad(HYP_SSR_PREV_IMAGE, coord) * divisor;
    accum_radius = imageLoad(ssr_radius_image, coord).r * divisor;

    GaussianBlurAccum(texcoord, 1, reflection_sample, accum_radius, divisor);
    GaussianBlurAccum(texcoord, -1, reflection_sample, accum_radius, divisor);
    
    if (divisor > 0.0) {
        reflection_sample /= divisor;
        accum_radius /= divisor;
    }
#else

    accum_radius  = imageLoad(ssr_radius_image, coord).r;
    const float blur_amount = mix(HYP_SSR_BLUR_MIN, HYP_SSR_MAX_BLUR_INCREMENT, Saturate(accum_radius * HYP_SSR_BLUR_MULTIPLIER));

    for (int x = -1; x <= 1; ++x) {
#ifdef HYP_SSR_BLUR_HORIZONTAL
        vec2 offset_coord = ivec2(x, 0);
#else
        vec2 offset_coord = ivec2(0, x);
#endif

        vec2 offset_texcoord = offset_coord * blur_amount / vec2(ssr_params.dimension);
        vec4 offset_reflection_sample = Texture2D(HYP_SAMPLER_LINEAR, HYP_SSR_PREV_TEXTURE, texcoord + offset_texcoord);

        reflection_sample += offset_reflection_sample;
    }

    reflection_sample /= 3.0;

#endif

#ifdef HYP_SSR_BLUR_VERTICAL
    reflection_sample.rgb = pow(reflection_sample.rgb, vec3(1.0 / 2.2));
#endif

    imageStore(ssr_blur, coord, reflection_sample);
}
