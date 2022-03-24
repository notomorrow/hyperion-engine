#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;
layout(location=2) in vec3 v_light_direction;
layout(location=3) in vec3 v_camera_position;

layout(location=0) out vec4 color_output;

layout(set = 1, binding = 0) uniform sampler2D gbuffer_albedo_ping;
layout(set = 1, binding = 1) uniform sampler2D gbuffer_normals_ping;
layout(set = 1, binding = 2) uniform sampler2D gbuffer_positions_ping;

layout(set = 2, binding = 0) uniform sampler2D filter_0;

//push constants block
layout( push_constant ) uniform constants
{
	layout(offset = 0) uint previous_frame_index;
    layout(offset = 4) uint current_frame_index;
    layout(offset = 8) uint material_index;
    
} push_constants;

struct Material {
    vec4 albedo;
    float metalness;
    float roughness;
};

struct Object {
    mat4 model_matrix;
};

layout(std430, set = 3, binding = 0) readonly buffer MaterialData {
    Material material;
} material_data;
layout(std430, set = 3, binding = 1) readonly buffer ObjectData {
    Object object;
} object_data;

layout(set = 0, binding = 3, rgba16f) uniform image2D image_storage_test;

void main()
{
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo;
    vec4 normal;
    vec4 position;
    
    albedo = texture(filter_0, texcoord);
    normal = texture(gbuffer_normals_ping, texcoord);
    position = texture(gbuffer_positions_ping, texcoord);
    
    float NdotL = dot(normal.xyz, v_light_direction);

    //color_output = (t0 + t1 + t2 + t3 + t4) / 5.0;//
    color_output = vec4(vec3(max(NdotL, 0.025)) * albedo.rgb * material_data.material.albedo.rgb, 1.0);
}