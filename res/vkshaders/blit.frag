#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;
layout(location=2) in vec3 v_light_direction;
layout(location=3) in vec3 v_camera_position;

layout(set = 1, binding = 0) uniform sampler2D gbuffer_albedo_texture;
layout(set = 1, binding = 1) uniform sampler2D gbuffer_normals_texture;
layout(set = 1, binding = 2) uniform sampler2D gbuffer_positions_texture;
layout(set = 1, binding = 3) uniform sampler2D gbuffer_depth_texture;

layout(set = 2, binding = 5) uniform sampler2D filter_0;
layout(set = 2, binding = 6) uniform sampler2D filter_1;

//layout(set = 0, binding = 3, rgba16f) uniform image2D image_storage_test;

layout(location=0) out vec4 out_color;

//push constants block
layout( push_constant ) uniform constants
{
	layout(offset = 0) uint previous_frame_index;
    layout(offset = 4) uint current_frame_index;
    layout(offset = 8) uint material_index;
    
} PushConstants;

void main() {
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo = vec4(0.0);

    /* render last filter in the stack */
    //out_color = imageLoad(image_storage_test, ivec2(int(v_texcoord0.x * 512.0), int(v_texcoord0.y * 512.0)));
    
    //if (out_color.a < 0.2) {
        out_color = texture(filter_1, texcoord);
    //}
}