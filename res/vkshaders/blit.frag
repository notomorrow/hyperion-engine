#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;
layout(location=2) in vec3 v_light_direction;
layout(location=3) in vec3 v_camera_position;

layout(location=0) out vec4 out_color;

//layout(set = 1, binding = 0) uniform sampler2D gbuffer_albedo;
//layout(set = 1, binding = 1) uniform sampler2D gbuffer_normals;
//layout(set = 1, binding = 2) uniform sampler2D gbuffer_positions;

layout(set = 2, binding = 0) uniform sampler2D gbuffer_albedo_0;
layout(set = 2, binding = 1) uniform sampler2D gbuffer_normals_0;
layout(set = 2, binding = 2) uniform sampler2D gbuffer_positions_0;
layout(set = 2, binding = 3) uniform sampler2D gbuffer_depth_0;
/*layout(set = 2, binding = 4) uniform sampler2D gbuffer_albedo_1;
layout(set = 2, binding = 5) uniform sampler2D gbuffer_normals_1;
layout(set = 2, binding = 6) uniform sampler2D gbuffer_positions_1;
layout(set = 2, binding = 7) uniform sampler2D gbuffer_depth_1;
layout(set = 2, binding = 8) uniform sampler2D gbuffer_albedo_2;
layout(set = 2, binding = 9) uniform sampler2D gbuffer_normals_2;
layout(set = 2, binding = 10) uniform sampler2D gbuffer_positions_2;
layout(set = 2, binding = 11) uniform sampler2D gbuffer_depth_2;*/


/*layout(set = 2, binding = 13) uniform sampler2D gbuffer_albedo_0;
layout(set = 2, binding = 14) uniform sampler2D gbuffer_normals_0;
layout(set = 2, binding = 15) uniform sampler2D gbuffer_positions_0;
layout(set = 2, binding = 16) uniform sampler2D gbuffer_depth_0;
layout(set = 2, binding = 17) uniform sampler2D gbuffer_albedo_1;
layout(set = 2, binding = 18) uniform sampler2D gbuffer_normals_1;
layout(set = 2, binding = 19) uniform sampler2D gbuffer_positions_1;
layout(set = 2, binding = 20) uniform sampler2D gbuffer_depth_1;
layout(set = 2, binding = 21) uniform sampler2D gbuffer_albedo_2;
layout(set = 2, binding = 22) uniform sampler2D gbuffer_normals_2;
layout(set = 2, binding = 23) uniform sampler2D gbuffer_positions_2;
layout(set = 2, binding = 24) uniform sampler2D gbuffer_depth_2;*/

//push constants block
layout( push_constant ) uniform constants
{
	layout(offset = 0) uint previous_frame_index;
    layout(offset = 4) uint current_frame_index;
} PushConstants;

void main() {
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo = vec4(0.0);

    // TODO: #define for max # of frames
    //if (PushConstants.current_frame_index == 0) {
        albedo = texture(gbuffer_albedo_0, texcoord);
    //} else if (PushConstants.current_frame_index == 1) {
    //    albedo = texture(gbuffer_normals_1, texcoord);
    //} else if (PushConstants.current_frame_index == 2) {
    //    albedo = texture(gbuffer_normals_2, texcoord);
    //}

    out_color = albedo;
}