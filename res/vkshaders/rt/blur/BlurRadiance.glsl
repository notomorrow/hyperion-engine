#include "BlurRadianceHeader.inc"

layout(set = 0, binding = 0, rgba8) uniform readonly image2D rt_radiance_image;
layout(set = 0, binding = 1, rgba8) uniform readonly image2D rt_normals_roughness_weight_image;
layout(set = 0, binding = 2, r32f) uniform readonly image2D rt_depth_image;
layout(set = 0, binding = 3) uniform texture2D blur_input_texture;
layout(set = 0, binding = 4) uniform sampler blur_input_sampler;
layout(set = 0, binding = 5, rgba8) uniform writeonly image2D blur_output_image;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/defines.inc"
#include "../../include/packing.inc"
#include "../../include/shared.inc"
#include "../../include/scene.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

//#define HYP_RADIANCE_USE_GAUSSIAN_BLUR

layout(std140, set = 0, binding = 10, row_major) readonly buffer SceneBuffer
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

    // temp: just writing radiance
    const ivec2 data_coord = ivec2(texcoord * vec2(imageSize(rt_normals_roughness_weight_image) - ivec2(1)));
    const vec4 data_value = imageLoad(rt_normals_roughness_weight_image, data_coord);

    const ivec2 depth_coord = ivec2(texcoord * vec2(imageSize(rt_depth_image) - ivec2(1)));
    const float depth_value = imageLoad(rt_depth_image, depth_coord).r;

    const vec4 world_position = ReconstructWorldSpacePositionFromDepth(inverse(scene.projection), inverse(scene.view), texcoord, depth_value);

    vec3 normal = UnpackNormalVec2(data_value.xy);
    float roughness = data_value.z;
    float weight = data_value.a * 255.0;

    const vec2 blur_input_texture_size = vec2(imageSize(rt_radiance_image));
    const float blur_input_texture_size_max = max(blur_input_texture_size.x, blur_input_texture_size.y);
    const float texel_size = 1.0 / max(blur_input_texture_size_max, HYP_FMATH_EPSILON);
    const float radius = texel_size  / max(depth_value, HYP_FMATH_EPSILON) * weight;

    // const ivec2 input_image_dimensions = imageSize(blur_input_image);
    // const ivec2 input_coord = ivec2(texcoord * vec2(input_image_dimensions - 1));
    
    vec4 output_color = vec4(0.0);

    float num_samples = 0.0;

    for (int i = -3; i <= 3; i++) {
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
    output_color = Texture2D(blur_input_sampler, blur_input_texture, texcoord);

    imageStore(blur_output_image, coord, vec4(output_color));
}
