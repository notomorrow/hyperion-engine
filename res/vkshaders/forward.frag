#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=6) in vec3 v_light_direction;
layout(location=7) in vec3 v_camera_position;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_positions;

//layout(binding = 2) uniform samplerCube tex;
layout(binding = 2) uniform sampler2D tex[2];

struct Material {
    vec4 albedo;
    float metalness;
    float roughness;
};

layout(std140, set = 3, binding = 0) readonly buffer MaterialBuffer {
    Material material;
};

layout(set = 6, binding = 0) uniform sampler2D textures[];

void main() {
    vec3 view_vector = normalize(v_position - v_camera_position);
    vec3 normal = normalize(v_normal);
    float NdotV = dot(normal, view_vector);
    float NdotL = dot(normal, normalize(v_light_direction));
    
    vec3 reflection_vector = reflect(view_vector, normal);
    
    gbuffer_albedo = texture(textures[1], v_texcoord0) * material.albedo;
    gbuffer_normals = vec4(normal, 1.0);
    gbuffer_positions = vec4(v_position, 1.0);
}