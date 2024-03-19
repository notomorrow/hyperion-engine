#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=4) in vec3 v_tangent;
layout(location=5) in vec3 v_bitangent;
layout(location=11) in vec4 v_position_ndc;
layout(location=12) in vec4 v_previous_position_ndc;
layout(location=15) in flat uint v_object_index;
#ifdef IMMEDIATE_MODE
layout(location=16) in vec4 v_color;
layout(location=17) in flat uint v_probe_id;
layout(location=18) in flat uint v_probe_type;
#endif

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_material;
layout(location=4) out vec2 gbuffer_velocity;
layout(location=5) out vec4 gbuffer_mask;
layout(location=6) out vec4 gbuffer_ws_normals;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#include "include/material.inc"
#include "include/packing.inc"
#include "include/env_probe.inc"
#include "include/scene.inc"

HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer, size = 512) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, ScenesBuffer, size = 256) readonly buffer ScenesBuffer
{
    Scene scene;
};

HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, count = 16) uniform textureCube env_probe_textures[16];
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, EnvGridsBuffer, size = 4352) uniform EnvGridsBuffer { EnvGrid env_grid; };
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer, size = 131072) readonly buffer EnvProbesBuffer { EnvProbe env_probes[HYP_MAX_ENV_PROBES]; };
HYP_DESCRIPTOR_SSBO(Scene, SHGridBuffer, size = 147456) readonly buffer SHGridBuffer { vec4 sh_grid_buffer[SH_GRID_BUFFER_SIZE]; };

#ifdef IMMEDIATE_MODE

#include "include/brdf.inc"
#include "deferred/DeferredLighting.glsl"

#else
#include "include/object.inc"

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, size = 33554432) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

// #ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Object, MaterialsBuffer, size = 8388608) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef HYP_FEATURES_BINDLESS_TEXTURES
HYP_DESCRIPTOR_SRV(Material, Textures, count = 16) uniform texture2D textures[HYP_MAX_BOUND_TEXTURES];
#else
HYP_DESCRIPTOR_SRV(Material, Textures) uniform texture2D textures[];
#endif

#ifndef CURRENT_MATERIAL
    #define CURRENT_MATERIAL (materials[object.material_index])
#endif
// #else

// HYP_DESCRIPTOR_SSBO_DYNAMIC(Object, MaterialsBuffer, size = 128) readonly buffer MaterialsBuffer
// {
//     Material material;
// };

// #ifndef CURRENT_MATERIAL
//     #define CURRENT_MATERIAL material
// #endif
// #endif

void main() {
    vec3 normal = normalize(v_normal);

    vec2 velocity = vec2(((v_position_ndc.xy / v_position_ndc.w) * 0.5 + 0.5) - ((v_previous_position_ndc.xy / v_previous_position_ndc.w) * 0.5 + 0.5));
    
    gbuffer_albedo = vec4(0.0, 1.0, 0.0, 1.0);
    gbuffer_normals = EncodeNormal(normal);
    gbuffer_material = vec4(0.0, 0.0, 0.0, 1.0);
    gbuffer_velocity = vec2(velocity);
    gbuffer_ws_normals = EncodeNormal(normal);

#ifdef IMMEDIATE_MODE
    gbuffer_albedo = vec4(v_color.rgb, 0.0);
    gbuffer_mask = UINT_TO_VEC4(0x400);

    if (v_probe_id != 0 && v_probe_type == ENV_PROBE_TYPE_REFLECTION) {
        const vec3 N = normal;
        const vec3 V = normalize(camera.position.xyz - v_position.xyz);

        vec4 ibl = vec4(0.0);

        const vec3 R = reflect(-V, N);

        ApplyReflectionProbe(env_probes[v_probe_id - 1], v_position.xyz, R, 0.0, ibl);

        gbuffer_albedo.rgb = ibl.rgb;
    }
#else
    gbuffer_mask = UINT_TO_VEC4(GET_OBJECT_BUCKET(object));
#endif
}
