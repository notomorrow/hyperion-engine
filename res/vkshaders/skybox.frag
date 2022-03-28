#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=6) in vec3 v_light_direction;
layout(location=7) in vec3 v_camera_position;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_positions;

layout(binding = 2) uniform samplerCube tex;

void main() {
    vec3 normal = normalize(v_normal);
    
    gbuffer_albedo = vec4(texture(tex, v_position).rgb, 1.0);
    gbuffer_normals = vec4(normal, 1.0);
    gbuffer_positions = vec4(v_position, 1.0);
}