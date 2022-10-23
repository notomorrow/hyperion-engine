#extension GL_GOOGLE_include_directive : require

#include "BlurRadianceHeader.inc"

layout(set = 0, binding = 0) uniform texture2D blur_input_texture;
layout(set = 0, binding = 1) uniform sampler blur_input_sampler;
layout(set = 0, binding = 2, rgba8) uniform writeonly image2D blur_output_image;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/defines.inc"
#include "../../include/packing.inc"
#include "../../include/shared.inc"
#include "../../include/scene.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

//#define HYP_RADIANCE_USE_GAUSSIAN_BLUR

layout(std140, set = 0, binding = 3, row_major) readonly buffer SceneBuffer
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

	const vec2 texcoord = vec2(coord) / vec2(output_dimensions);
    float weight = 1.0;

    // const vec2 blur_input_texture_size = vec2(imageSize(input_image));
    const float output_dimension_max = float(max(output_dimensions.x, output_dimensions.y));
    const float texel_size = 1.0 / max(output_dimension_max, HYP_FMATH_EPSILON);
    const float radius = texel_size * weight;

    // const ivec2 input_image_dimensions = imageSize(blur_input_image);
    // const ivec2 input_coord = ivec2(texcoord * vec2(input_image_dimensions - 1));
    
    vec4 output_color = vec4(0.0);

    float num_samples = 0.0;

    for (int i = -1; i <= 1; i++) {
#ifdef HYP_RADIANCE_BLUR_HORIZONTAL
        ivec2 offset = ivec2(0, i);
#else
        ivec2 offset = ivec2(i, 0);
#endif

        // ivec2 offset_coord = clamp(input_coord + offset, ivec2(0), ivec2(input_image_dimensions - 1));
        vec2 offset_texcoord = Saturate(texcoord + (vec2(offset) * radius));
        const vec4 input_color = Texture2D(blur_input_sampler, blur_input_texture, offset_texcoord);

        output_color += input_color;

        num_samples += 1.0;
    }

    output_color /= max(num_samples, 1.0);

#ifdef HYP_RADIANCE_BLUR_VERTICAL
    // output_color.rgb = pow(output_color.rgb, vec3(1.0 / 2.2));
#endif

    // temp
    // output_color = Texture2D(blur_input_sampler, blur_input_texture, texcoord);

    imageStore(blur_output_image, coord, vec4(output_color));
}
