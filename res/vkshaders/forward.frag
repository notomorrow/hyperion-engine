#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=6) in vec3 v_light_direction;
layout(location=7) in vec3 v_camera_position;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_positions;

struct Material {
    vec4 albedo;
    
    float metalness;
    float roughness;
    float subsurface;
    float specular;
    
    float specular_tint;
    float anisotropic;
    float sheen;
    float sheen_tint;
    
    float clearcoat;
    float clearcoat_gloss;
    float emissiveness;
    float _padding0;
    
    uint uv_flip_s;
    uint uv_flip_t;
    float uv_scale;
    float parallax_height;
    
    uint texture_index[2];
    uint texture_usage[2];
    
    /* Texture schema:
       0 - albedo
       1 - normals
       2 - ao
       3 - parallax
       4 - metalness
       5 - roughness */
};

layout(std430, set = 3, binding = 0) readonly buffer MaterialBuffer {
    Material material;
};

layout(set = 6, binding = 0) uniform sampler2D textures[];

void main() {
    vec3 view_vector = normalize(v_position - v_camera_position);
    vec3 normal = normalize(v_normal);
    float NdotV = dot(normal, view_vector);
    float NdotL = dot(normal, normalize(v_light_direction));
    
    vec3 reflection_vector = reflect(view_vector, normal);
    
    gbuffer_albedo = material.albedo;
    gbuffer_albedo *= bool(material.texture_usage[0])
        ? texture(textures[material.texture_index[0]], v_texcoord0)
        : vec4(1.0);
    
    gbuffer_normals = vec4(normal, 1.0);
    gbuffer_positions = vec4(v_position, 1.0);
}