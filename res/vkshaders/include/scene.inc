#include "defines.inc"

struct Scene
{
    mat4 view;
    mat4 projection;

    vec4 camera_position;
    vec4 camera_direction;
    vec4 camera_up;

    float camera_near;
    float camera_far;
    float camera_fov;
    float _pad0;
    
    uint environment_texture_index;
    uint environment_texture_usage;
    uint resolution_x;
    uint resolution_y;
    
    vec4 aabb_max;
    vec4 aabb_min;

    float global_timer;
    uint num_environment_shadow_maps;
    uint num_lights;
    uint enabled_render_components_mask;
};

struct Light
{
    uint id;
    uint type;
    uint color_encoded;
    float radius;
    uint shadow_map_index; // ~0 == no shadow map

    uint _pad0, _pad1, _pad2, _pad3,
        _pad4, _pad5, _pad6;

    vec4 position_intensity; // or direction for directional lights
};

struct ShadowMap {
    mat4 projection;
    mat4 view;
    uint scene_index;
};

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 0, row_major) readonly buffer SceneShaderData
{
    Scene scene;
};

layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 1) readonly buffer LightShaderData
{
    Light light;
};

layout(set = HYP_DESCRIPTOR_SET_SCENE, binding = 12) uniform sampler2D shadow_maps[];

layout(std430, set = HYP_DESCRIPTOR_SET_SCENE, binding = 13, row_major) uniform ShadowShaderData
{
    ShadowMap matrices[113];
} shadow_data;


// RenderComponentName enum
#define HYP_RENDER_COMPONENT_VCT 0
#define HYP_RENDER_COMPONENT_SHADOWS 1
#define HYP_RENDER_COMPONENT_CUBEMAP 2
#define HYP_RENDER_COMPONENT_SVO 3
#define HYP_RENDER_COMPONENT_UI 4

bool IsRenderComponentEnabled(uint name)
{
    return bool(scene.enabled_render_components_mask & (1 << name));
}

#endif