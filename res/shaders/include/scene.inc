#ifndef HYP_SCENE_GLSL
#define HYP_SCENE_GLSL

#include "defines.inc"

struct WorldShaderData
{
    vec4 fog_params;

    float game_time;
    uint frame_counter;
    uint _pad0;
    uint _pad1;
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
    uint id;

    vec4 _pad1;
    vec4 _pad2;
    vec4 _pad3;

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
    vec2 area_size;        // for area lights

    vec4 position_intensity; // position or direction
    vec4 normal;             // for area lights

    vec2 spot_angles;    // for spot lights (x = outer, y = inner)
    uint material_index; // for area lights - ~0u == no material
    uint _pad2;

    vec4 aabb_min;
    vec4 aabb_max;
    uvec4 _pad5;
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

    vec4 dimensions_scale; // xy = slice dimensions in pixels, zw = slice dimensions relative to the atlas dimensions
    vec2 offset_uv;
    uint layer_index; // atlas index, also used for point light shadow map index
    uint flags;

    vec4 _pad1;
    vec4 _pad2;
    vec4 _pad3;
    vec4 _pad4;
};

struct LightmapVolume
{
    vec4 aabb_max;
    vec4 aabb_min;

    uint texture_index; // index of the lightmap texture in the lightmap atlas
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

vec3 CalculateLightDirection(Light light, in vec3 world_position)
{
    vec3 L = light.position_intensity.xyz;
    L -= world_position.xyz * float(min(light.type, 1));
    L = normalize(L);

    return L;
}

#endif