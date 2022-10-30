#extension GL_GOOGLE_include_directive : require

#include "BlurRadianceHeader.inc"

layout(set = 0, binding = 0) uniform texture2D blur_input_texture;
layout(set = 0, binding = 1) uniform texture2D prev_blur_input_texture;
layout(set = 0, binding = 2) uniform texture2D velocity_texture;
layout(set = 0, binding = 3) uniform sampler sampler_linear;
layout(set = 0, binding = 4) uniform sampler sampler_nearest;
layout(set = 0, binding = 5, rgba8) uniform writeonly image2D blur_output_image;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/defines.inc"
#include "../../include/packing.inc"
#include "../../include/shared.inc"
#include "../../include/scene.inc"
#include "../../include/Temporal.glsl"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

//#define HYP_RADIANCE_USE_GAUSSIAN_BLUR

layout(std140, set = 0, binding = 6, row_major) readonly buffer SceneBuffer
{
    Scene scene;
};

void main(void)
{
    const ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    const ivec2 output_dimensions = imageSize(blur_output_image);

    if (any(greaterThanEqual(coord, output_dimensions))) {
        return;
    }

	const vec2 texcoord = (vec2(coord) + 0.5) / vec2(output_dimensions);

    vec4 output_color = Texture2D(sampler_linear, blur_input_texture, texcoord);

    float weight = 0.25;

    // const vec2 blur_input_texture_size = vec2(imageSize(input_image));
    const float output_dimension_max = float(max(output_dimensions.x, output_dimensions.y));
    const float texel_size = 1.0 / max(output_dimension_max, HYP_FMATH_EPSILON);
    const float radius = texel_size * weight;

    // const ivec2 input_image_dimensions = imageSize(blur_input_image);
    // const ivec2 input_coord = ivec2(texcoord * vec2(input_image_dimensions - 1));
    

    float num_samples = 0.0;

    for (int i = -1; i <= 1; i++) {
#ifdef HYP_RADIANCE_BLUR_HORIZONTAL
        ivec2 offset = ivec2(0, i);
#else
        ivec2 offset = ivec2(i, 0);
#endif

        // ivec2 offset_coord = clamp(input_coord + offset, ivec2(0), ivec2(input_image_dimensions - 1));
        vec2 offset_texcoord = Saturate(texcoord + (vec2(offset) * radius));
        const vec4 input_color = Texture2D(sampler_linear, blur_input_texture, offset_texcoord);

        output_color += input_color;

        num_samples += 1.0;
    }

    output_color /= max(num_samples, 1.0);

    imageStore(blur_output_image, coord, vec4(output_color));
}
