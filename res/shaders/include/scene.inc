#include "defines.inc"

struct Scene
{
    vec4 aabb_max;
    vec4 aabb_min;
    vec4 fog_params;

    float global_timer;
    uint frame_counter;
    uint enabled_render_components_mask;
    float _pad0;

    mat4 _pad1;
    mat4 _pad2;
    mat4 _pad3;
};

struct Camera
{
    mat4 view;
    mat4 projection;
    mat4 previous_view;

    uvec4 dimensions;
    vec4 position;
    vec4 direction;
    vec4 jitter;

    float near;
    float far;
    float fov;
    float _pad0;

    vec4 _pad1; vec4 _pad2; vec4 _pad3;

    mat4 _pad4;
    mat4 _pad5;
    mat4 _pad6;
};

struct Light
{
    uint id;
    uint type;
    uint color_encoded;
    float radius;

    float falloff;
    uint shadow_map_index; // ~0 == no shadow map
    uint _pad0, _pad1;

    vec4 position_intensity; // or direction for directional lights

    vec4 _pad2; vec4 _pad3; vec4 _pad4;
};

#define SHADOW_FLAGS_PCF 0x1
#define SHADOW_FLAGS_VSM 0x2
#define SHADOW_FLAGS_CONTACT_HARDENED 0x4
#define SHADOW_FLAGS_TYPE_MASK (SHADOW_FLAGS_PCF | SHADOW_FLAGS_VSM | SHADOW_FLAGS_CONTACT_HARDENED)

struct ShadowMap
{
    mat4 projection;
    mat4 view;
    vec4 aabb_max;
    vec4 aabb_min;

    uvec2 dimensions;
    uint flags;
    float _pad2;

    vec4 _pad3; vec4 _pad4;
    vec4 _pad5; vec4 _pad6;
    vec4 _pad7;
};

vec3 CalculateLightDirection(Light light, in vec3 world_position)
{
    vec3 L = light.position_intensity.xyz;
    L -= world_position.xyz * float(min(light.type, 1));
    L = normalize(L);

    return L;
}

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 0, row_major) readonly buffer SceneShaderData
{
    Scene scene;
};

layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 1) readonly buffer LightShaderData
{
    Light light;
};

layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 4, row_major) uniform CameraShaderData
{
    Camera camera;
};

layout(set = HYP_DESCRIPTOR_SET_SCENE, binding = 12) uniform texture2D shadow_maps[];

layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 13, row_major) readonly buffer ShadowShaderData
{
    ShadowMap shadow_map_data[HYP_MAX_SHADOW_MAPS];
};

#endif