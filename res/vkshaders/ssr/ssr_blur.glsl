#include "ssr_header.inc"
#include "../include/noise.inc"
#include "../include/shared.inc"

#define SSR_BLUR_KERNEL_SIZE 8
#define HYP_SSR_FILTER_AMOUNT 0.0055

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 17) uniform texture2D ssr_uvs;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 18) uniform texture2D ssr_sample;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 19) uniform texture2D ssr_radius;

#if defined(HYP_SSR_BLUR_HORIZONTAL)
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 15, rgba16f) uniform writeonly image2D ssr_blur;
    #define HYP_SSR_PREV_IMAGE ssr_sample
#elif defined(HYP_SSR_BLUR_VERTICAL)
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 20) uniform texture2D ssr_blur_hor;
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 16, rgba16f) uniform writeonly image2D ssr_blur;
    #define HYP_SSR_PREV_IMAGE ssr_blur_hor
#else
    #error No blur direction defined
#endif

#define GAUSS_TABLE_SIZE 15

const float gauss_table[GAUSS_TABLE_SIZE + 1] = float[](
    0.1847392078702266,
    0.16595854345772326,
    0.12031364177766891,
    0.07038755277896766,
    0.03322925565155569,
    0.012657819729901945,
    0.0038903040680094217,
    0.0009646503390864025,
    0.00019297087402915717,
    0.000031139936308099136,
    0.000004053309048174758,
    4.255228059965837e-7,
    3.602517634249573e-8,
    2.4592560765896795e-9,
    1.3534945386863618e-10,
    0.0 //one more for interpolation
);

float GaussianWeight(float value)
{
    float idxf;
    float c = modf(max(0.0, value * float(GAUSS_TABLE_SIZE)), idxf);
    int idx = int(idxf);
    if (idx >= GAUSS_TABLE_SIZE + 1) {
        return 0.0;
    }

    return mix(gauss_table[idx], gauss_table[idx + 1], c);
}

void GaussianBlurAccum(
    vec2 texcoord,
    float direction,
    inout vec4 reflection_sample,
    inout float accum_radius,
    inout float divisor
)
{
    for (int i = 1; i < SSR_BLUR_KERNEL_SIZE; i++) {
        const float d = float(i * HYP_SSR_FILTER_AMOUNT);

#if defined(HYP_SSR_BLUR_HORIZONTAL)
        const vec2 blur_step = vec2(d, 0);
#elif defined(HYP_SSR_BLUR_VERTICAL)
        const vec2 blur_step = vec2(0, d);
#else
    #error No direction defined
#endif


#ifdef HYP_SSR_BLUR_VERTICAL
        const float inv_aspect = float(ssr_params.dimension.y) / float(ssr_params.dimension.x);
#endif

        vec2 tc = texcoord + blur_step * direction;

#ifdef HYP_SSR_BLUR_VERTICAL
        //tc *= inv_aspect;
#endif

        const float sample_radius = Texture2D(gbuffer_sampler, ssr_radius, tc).r;

        if (d < sample_radius) {
            float weight = GaussianWeight(d / sample_radius);

            reflection_sample += Texture2D(gbuffer_sampler, HYP_SSR_PREV_IMAGE, tc) * weight;


            divisor += d;

            accum_radius += sample_radius;
        }
    }
}

void main(void)
{
    const ivec2 coord    = ivec2(gl_GlobalInvocationID.xy);
	const vec2  texcoord = vec2(coord) / vec2(ssr_params.dimension.x, ssr_params.dimension.y);

    const float gradient_noise = InterleavedGradientNoise(vec2(coord));

    vec4 reflection_sample = Texture2D(gbuffer_sampler, HYP_SSR_PREV_IMAGE, texcoord);

    float accum_radius = Texture2D(gbuffer_sampler, ssr_radius, texcoord).r;

    float divisor = gauss_table[0];
    reflection_sample *= divisor;
    accum_radius      *= divisor;

    GaussianBlurAccum(texcoord, 1.0, reflection_sample, accum_radius, divisor);
    GaussianBlurAccum(texcoord, -1.0, reflection_sample, accum_radius, divisor);
    
    if (divisor > 0.0) {
        reflection_sample /= divisor;
        accum_radius      /= divisor;
    } else {
        reflection_sample = vec4(0.0);
        accum_radius      = 0.0;
    }


    // reflection_sample /= float(SSR_BLUR_KERNEL_SIZE);

    // imageStore(ssr_blur, coord, vec4(blur_radius, 0.0, 0.0, 1.0));

    imageStore(ssr_blur, coord, reflection_sample);
}
