#include "defines.inc"

#define HYP_MAX_SHADOW_MAPS 1 // tmp

struct Scene {
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
    // 232
    uint num_lights;

    // 236
    float camera_near;
    // 240
    float camera_far;
};

struct Light {
    vec4 position; // or direction for directional lights
    uint color_encoded;
    uint type;
    float intensity;
    float radius;
    uint shadow_map_index; // ~0 == no shadow map
};

struct ShadowMap {
    mat4 projection;
    mat4 view;
    uint scene_index;
};

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
layout(std430, set = HYP_DESCRIPTOR_SET_SCENE, binding = 0, row_major) readonly buffer SceneDataBlock {
    Scene scene;
};

layout(std430, set = HYP_DESCRIPTOR_SET_SCENE, binding = 1) readonly buffer LightDataBlock {
    Light light;
};

layout(set = HYP_DESCRIPTOR_SET_SCENE, binding = 12) uniform sampler2D shadow_maps[];

layout(std430, set = HYP_DESCRIPTOR_SET_SCENE, binding = 13, row_major) uniform ShadowData {
    ShadowMap matrices[113];
} shadow_data;

#endif