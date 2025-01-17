#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_screen_space_position;
layout(location=2) in vec2 v_texcoord0;
layout(location=3) in flat uint v_object_index;
layout (location=4) in vec4 v_color;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=5) out vec4 gbuffer_mask;

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/material.inc"
#include "../include/object.inc"
#include "../include/UIObject.glsl"
#include "../include/scene.inc"

// temp
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, size = 33554432) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;

#ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Object, MaterialsBuffer, size = 8388608) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef CURRENT_MATERIAL
    #define CURRENT_MATERIAL (materials[object.material_index])
#endif
#else

HYP_DESCRIPTOR_SSBO_DYNAMIC(Object, MaterialsBuffer, size = 128) readonly buffer MaterialsBuffer
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

    vec4 background = Texture2DLod(gbuffer_sampler, gbuffer_mip_chain, v_screen_space_position.xy, 4.0);
    background += Texture2DLod(gbuffer_sampler, gbuffer_mip_chain, v_screen_space_position.xy, 5.0);
    background += Texture2DLod(gbuffer_sampler, gbuffer_mip_chain, v_screen_space_position.xy, 6.0);
    background += Texture2DLod(gbuffer_sampler, gbuffer_mip_chain, v_screen_space_position.xy, 7.0);
    background *= 0.25;

    vec4 ui_color = CURRENT_MATERIAL.albedo;
    // allow some of the actual ui background to show through
    // vec4 background_blended = mix(background, vec4(ui_color.rgb, 0.0), 0.3);
    // ui_color = (vec4(ui_color.rgb, 1.0) * ui_color.a) + (background * (1.0 - ui_color.a));

    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map)) {
        // ivec2 texture_size = textureSize(sampler2D(GET_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map), texture_sampler), 0);
        vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, v_texcoord0);
        
        ui_color *= albedo_texture;
    }
    
    if (properties.border_radius > 0.0) {
        // rounded corners
        vec2 size = vec2(properties.size);
        vec2 position = v_texcoord0 * size;

        float roundedness = RoundedRectangle((size * 0.5) - position, size * 0.5, properties.border_radius);
        
        bool top = bool(properties.border_flags & UOB_TOP);
        bool left = bool(properties.border_flags & UOB_LEFT);
        bool bottom = bool(properties.border_flags & UOB_BOTTOM);
        bool right = bool(properties.border_flags & UOB_RIGHT);

        roundedness = top ? roundedness : mix(roundedness, 1.0, step(0.5, 1.0 - v_texcoord0.y));
        roundedness = left ? roundedness : mix(roundedness, 1.0, step(0.5, 1.0 - v_texcoord0.x));
        roundedness = bottom ? roundedness : mix(roundedness, 1.0, step(0.5, v_texcoord0.y));
        roundedness = right ? roundedness : mix(roundedness, 1.0, step(0.5, v_texcoord0.x));

        ui_color.a *= roundedness;
    }
// #if defined(TYPE_BUTTON) || defined(TYPE_TAB) || defined(TYPE_PANEL)
//     // If focused show a border around the object
//     // TEMP Testing.
//     if (bool(properties.focus_state & UOFS_FOCUSED)) {
//         ui_color = mix(ui_color, vec4(1.0, 1.0, 1.0, 1.0), 0.5);
//     }
// #endif

    uint mask = GET_OBJECT_BUCKET(object);

    // gbuffer_albedo = vec4(vec3(bvec3(HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map))), 1.0);//ui_color;
    // gbuffer_albedo = vec4(vec3(bvec3((CURRENT_MATERIAL).texture_indices[(MATERIAL_TEXTURE_ALBEDO_map / 4)][MATERIAL_TEXTURE_ALBEDO_map % 4] != 0)), 1.0);//ui_color;
    // gbuffer_albedo = vec4(vec3(float(TextureSize(HYP_SAMPLER_LINEAR, GET_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map)).x) * 0.0005), 1.0);//ui_color;
    // gbuffer_albedo.rgb = UINT_TO_VEC4(object.material_index).rgb;//ui_color;
    // gbuffer_albedo.a = 1.0;
    gbuffer_albedo = ui_color * v_color;
// #ifdef TYPE_TEXT
//     gbuffer_albedo = vec4(v_texcoord0, 0.0, 1.0);
// #endif
    // gbuffer_albedo = vec4(UINT_TO_VEC4(v_object_index).rgb * 10.0, 1.0);
    gbuffer_mask = UINT_TO_VEC4(mask);
}
