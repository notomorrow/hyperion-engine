#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=6) in vec3 v_light_direction;
layout(location=7) in vec3 v_camera_position;

layout(location=0) out vec4 outColor;

layout(binding = 2) uniform MaterialBlock {
    vec3 camera_position;
    vec3 light_direction;
} Material;

layout(set = 1, binding = 0) uniform sampler2D fboTex;

void main() {
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 normal = normalize(v_normal);
    float NdotV = dot(normal, view_vector);
    float NdotL = dot(normal, v_light_direction);
    vec3 half_vector = normalize(v_light_direction + view_vector);
    float NdotH = dot(normal, half_vector);
    float blinn = pow(NdotH, 20.0) * NdotL;
    
    vec3 reflection_vector = reflect(view_vector, normal);

    outColor = vec4(1.0, 0.0, 0.0, 1.0);//vec4(texture(fboTex, vec2(v_texcoord0.x, 1.0 - v_texcoord0.y)).rgb, 1.0);
    //outColor = vec4(vec3(NdotL), 1.0);
}