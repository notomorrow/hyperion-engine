#include "ssr_header.inc"
#include "../include/noise.inc"
#include "../include/shared.inc"

#define HYP_SSR_MAX_BLUR_INCREMENT 1

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 14, r8) uniform image2D ssr_radius_image;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 13, rgba16f) uniform readonly image2D ssr_sample_image;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 18) uniform texture2D ssr_sample;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 19) uniform texture2D ssr_radius;

#if defined(HYP_SSR_BLUR_HORIZONTAL)
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 15, rgba16f) uniform writeonly image2D ssr_blur;
    #define HYP_SSR_PREV_IMAGE ssr_sample_image
#elif defined(HYP_SSR_BLUR_VERTICAL)
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 20) uniform texture2D ssr_blur_hor;
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 15, rgba16f) uniform readonly image2D ssr_blur_hor_image;
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 16, rgba16f) uniform writeonly image2D ssr_blur;
    #define HYP_SSR_PREV_IMAGE ssr_blur_hor_image
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
    int direction,
    inout vec4 reflection_sample,
    inout float accum_radius,
    inout float divisor
)
{
    for (int i = 1; i < GAUSS_TABLE_SIZE; i++) {
        const float d = float(i * HYP_SSR_MAX_BLUR_INCREMENT);

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

void main(void)
{
    const ivec2 coord    = ivec2(gl_GlobalInvocationID.xy);
	const vec2  texcoord = vec2(coord) / vec2(ssr_params.dimension.x, ssr_params.dimension.y);

    const float gradient_noise = InterleavedGradientNoise(vec2(coord));

    // const float roughness = SampleGBuffer(gbuffer_material_texture, texcoord).r;

    vec4 reflection_sample = vec4(0.0);
    float accum_radius     = 0.0;

    // if (roughness < HYP_SSR_ROUGHNESS_MAX) {
        float divisor = gauss_table[0];

        reflection_sample = imageLoad(HYP_SSR_PREV_IMAGE, coord) * divisor;
        accum_radius      = imageLoad(ssr_radius_image, coord).r * divisor;

        GaussianBlurAccum(texcoord, 1, reflection_sample, accum_radius, divisor);
        GaussianBlurAccum(texcoord, -1, reflection_sample, accum_radius, divisor);
        
        if (divisor > 0.0) {
            reflection_sample /= divisor;
            accum_radius      /= divisor;
        }
    // }

#ifdef HYP_SSR_BLUR_VERTICAL
    reflection_sample.rgb = pow(reflection_sample.rgb, vec3(1.0 / 2.2));
#endif

    imageStore(ssr_blur, coord, reflection_sample);
}
