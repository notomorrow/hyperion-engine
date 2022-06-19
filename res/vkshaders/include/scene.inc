#define HYP_MAX_SHADOW_MAPS 1 // tmp

layout(std140, set = 2, binding = 0, row_major) readonly buffer SceneDataBlock {
    // 0
    mat4 view;
    mat4 projection;
    // 128
    vec4 camera_position;
    // 144
    vec4 camera_direction;
    // 160
    vec4 light_direction;
    
    // 176
    uint environment_texture_index;
    uint environment_texture_usage;
    uint resolution_x;
    uint resolution_y;
    
    // 192
    vec4 aabb_max;
    vec4 aabb_min;

    // 224
    float global_timer;

    // 228
    uint num_environment_shadow_maps;
} scene;

struct Light {
    vec4 position; // or direction for directional lights
    uint color_encoded;
    uint type;
    float intensity;
    uint shadow_map_index; // ~0 == no shadow map
};

layout(std430, set = 2, binding = 1) readonly buffer LightDataBlock {
    Light lights[];
};

layout(set = 2, binding = 12) uniform sampler2D shadow_maps[];

struct ShadowMap {
    mat4 projection;
    mat4 view;
    uint scene_index;
};

layout(std430, set = 2, binding = 13, row_major) uniform ShadowData {
    ShadowMap matrices[113];
} shadow_data;

bool HasEnvironmentTexture(uint index)
{
    return bool(scene.environment_texture_usage & (1 << index));
}
