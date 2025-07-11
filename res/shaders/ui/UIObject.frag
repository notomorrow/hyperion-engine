#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_screen_space_position;
layout(location = 2) in vec2 v_texcoord0;
layout(location = 3) in vec4 v_color;

#ifdef INSTANCING
layout(location = 4) in flat uint v_object_index;
#endif

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 5) out vec4 gbuffer_mask;

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/material.inc"
#include "../include/object.inc"
#include "../include/UIObject.glsl"
#include "../include/scene.inc"

// temp
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

#ifdef INSTANCING

HYP_DESCRIPTOR_SSBO(Global, ObjectsBuffer) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

#else

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, ObjectsBuffer) readonly buffer ObjectsBuffer
{
    Object object;
};

#endif

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear)
uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest)
uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

#ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Object, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL (materials[object.material_index])
#endif
#else

HYP_DESCRIPTOR_SSBO_DYNAMIC(Object, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material material;
};

#ifndef CURRENT_MATERIAL
#define CURRENT_MATERIAL material
#endif
#endif

#ifndef HYP_FEATURES_BINDLESS_TEXTURES
HYP_DESCRIPTOR_SRV(Material, Textures, count = 16) uniform texture2D textures[HYP_MAX_BOUND_TEXTURES];
#else
HYP_DESCRIPTOR_SRV(Material, Textures) uniform texture2D textures[];
#endif

float RoundedRectangle(vec2 pos, vec2 size, float radius)
{
    return 1.0 - clamp((length(max(abs(pos) - size + radius, 0.0)) - radius), 0.0, 1.0);
}

void main()
{
    UIObjectProperties properties;
    GetUIObjectProperties(object, properties);

    vec4 ui_color = CURRENT_MATERIAL.albedo;

    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map))
    {
        vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, v_texcoord0);

        ui_color *= albedo_texture;
    }

    // rounded corners
    vec2 size = vec2(properties.size);
    vec2 position = v_texcoord0 * size;

    float roundedness = RoundedRectangle((size * 0.5) - position, size * 0.5, properties.border_radius);

    float top = float((properties.border_flags & UOB_TOP) != 0u);
    float left = float((properties.border_flags & UOB_LEFT) != 0u);
    float bottom = float((properties.border_flags & UOB_BOTTOM) != 0u);
    float right = float((properties.border_flags & UOB_RIGHT) != 0u);

    roundedness = mix(mix(roundedness, 1.0, step(0.5, 1.0 - v_texcoord0.y)), roundedness, top);
    roundedness = mix(mix(roundedness, 1.0, step(0.5, 1.0 - v_texcoord0.x)), roundedness, left);
    roundedness = mix(mix(roundedness, 1.0, step(0.5, v_texcoord0.y)), roundedness, bottom);
    roundedness = mix(mix(roundedness, 1.0, step(0.5, v_texcoord0.x)), roundedness, right);

    ui_color.a *= mix(1.0, roundedness, 1.0 - step(properties.border_radius, 0.0));

    uint mask = GET_OBJECT_BUCKET(object) | OBJECT_MASK_UI;

    gbuffer_albedo = ui_color * v_color;
    gbuffer_mask = UINT_TO_VEC4(mask);
}
