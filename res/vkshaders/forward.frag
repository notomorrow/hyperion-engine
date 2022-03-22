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

//layout(binding = 2) uniform samplerCube tex;
layout(binding = 2) uniform sampler2D tex[2];

void main() {
    vec3 view_vector = normalize(v_position - v_camera_position);
    vec3 normal = normalize(v_normal);
    float NdotV = dot(normal, view_vector);
    float NdotL = dot(normal, normalize(v_light_direction));
    
    vec3 reflection_vector = reflect(view_vector, normal);
    
    gbuffer_albedo = vec4(textureLod(tex[1], vec2(v_texcoord0.x, 1.0 - v_texcoord0.y), 7).rgb, 1.0); //vec4(1.0, 0.0, 0.0, 1.0);  //texture(tex, reflection_vector).rgb, 1.0);//
    gbuffer_normals = vec4(normal, 1.0);
    gbuffer_positions = vec4(v_position, 1.0);
}