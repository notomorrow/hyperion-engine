#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

#define HYP_NO_CUBEMAP

#include "../../include/defines.inc"
#include "../../include/vertex.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/material.inc"
#include "../../include/object.inc"
#include "../../include/scene.inc"
#include "../../include/noise.inc"

#include "../../include/brdf.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/mesh.inc"

#define PATHTRACER
#include "../../include/rt/payload.inc"

HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

#define HYP_SAMPLER_NEAREST sampler_nearest
#define HYP_SAMPLER_LINEAR sampler_linear

/* Shadows */

HYP_DESCRIPTOR_SRV(Scene, ShadowMapTextures, count = 16) uniform texture2D shadow_maps[HYP_MAX_SHADOW_MAPS];

HYP_DESCRIPTOR_UAV(Scene, ShadowMapsBuffer, size = 4096) readonly buffer ShadowMapsBuffer
{
    ShadowMap shadow_map_data[HYP_MAX_SHADOW_MAPS];
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/shadows.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/RTRadiance.inc"

/* End Shadows */

#undef HYP_NO_CUBEMAP

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

struct PackedVertex
{
    float position_x;
    float position_y;
    float position_z;
    float normal_x;
    float normal_y;
    float normal_z;
    float texcoord_s;
    float texcoord_t;
};

layout(buffer_reference, scalar) readonly buffer PackedVertexBuffer { float vertices[]; };
layout(buffer_reference, scalar) readonly buffer IndexBuffer { uvec3 indices[]; };

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer) readonly buffer ObjectsBuffer
{
    Object entities[];
};

HYP_DESCRIPTOR_SSBO(RTRadianceDescriptorSet, MeshDescriptionsBuffer) buffer MeshDescriptions
{
    MeshDescription mesh_descriptions[];
};

HYP_DESCRIPTOR_SSBO(RTRadianceDescriptorSet, MaterialsBuffer) readonly buffer MaterialBuffer
{
    Material materials[];
};

HYP_DESCRIPTOR_SSBO(RTRadianceDescriptorSet, LightsBuffer) readonly buffer LightsBuffer
{
    Light lights[];
};

HYP_DESCRIPTOR_CBUFF(RTRadianceDescriptorSet, RTRadianceUniforms) uniform RTRadianceUniformBuffer
{
    RTRadianceUniforms rt_radiance_uniforms;
};

// for RT, all textures are bindless
HYP_DESCRIPTOR_SRV(Material, Textures) uniform sampler2D textures[];

float CheckLightIntersection(in Light light, in vec3 position, in vec3 R)
{
    vec3 L = CalculateLightDirection(light, position);

    if (light.type == 0) { // directional handled by rmiss
        return HYP_FMATH_INFINITY;
    }

    vec3 light_to_position = position - light.position_intensity.xyz;
    float light_to_position_length = length(light_to_position);

    if (light_to_position_length > light.radius) {
        return HYP_FMATH_INFINITY;
    }

    return light_to_position_length;
}

void main()
{
    MeshDescription mesh_description = mesh_descriptions[gl_InstanceCustomIndexEXT];
    PackedVertexBuffer vertex_buffer = PackedVertexBuffer(mesh_description.vertex_buffer_address);
    IndexBuffer index_buffer = IndexBuffer(mesh_description.index_buffer_address);

    uvec3 index = uvec3(
        index_buffer.indices[gl_PrimitiveID]
    );
    // payload.color = vec3(greaterThan(index, ivec3(mesh_description.num_vertices)));
    // return;
    Vertex v0;
    {
        const uint offset = 8 * index[0];
        v0.position = vec3(
            vertex_buffer.vertices[offset],
            vertex_buffer.vertices[offset + 1],
            vertex_buffer.vertices[offset + 2]
        );
        
        v0.normal = vec3(
            vertex_buffer.vertices[offset + 3],
            vertex_buffer.vertices[offset + 4],
            vertex_buffer.vertices[offset + 5]
        );
        
        v0.texcoord0 = vec2(
            vertex_buffer.vertices[offset + 6],
            vertex_buffer.vertices[offset + 7]
        );
    }

    Vertex v1;
    {
        const uint offset = 8 * index[1];
        v1.position = vec3(
            vertex_buffer.vertices[offset],
            vertex_buffer.vertices[offset + 1],
            vertex_buffer.vertices[offset + 2]
        );
        
        v1.normal = vec3(
            vertex_buffer.vertices[offset + 3],
            vertex_buffer.vertices[offset + 4],
            vertex_buffer.vertices[offset + 5]
        );
        
        v1.texcoord0 = vec2(
            vertex_buffer.vertices[offset + 6],
            vertex_buffer.vertices[offset + 7]
        );
    }
    Vertex v2;
    {
        const uint offset = 8 * index[2];
        v2.position = vec3(
            vertex_buffer.vertices[offset],
            vertex_buffer.vertices[offset + 1],
            vertex_buffer.vertices[offset + 2]
        );
        
        v2.normal = vec3(
            vertex_buffer.vertices[offset + 3],
            vertex_buffer.vertices[offset + 4],
            vertex_buffer.vertices[offset + 5]
        );
        
        v2.texcoord0 = vec2(
            vertex_buffer.vertices[offset + 6],
            vertex_buffer.vertices[offset + 7]
        );
    }
    // payload.color = vec3(v0.position);
    // return;

    // Interpolate normal
    const vec3 barycentric_coords = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    const vec3 normal = normalize((gl_ObjectToWorldEXT * vec4(v0.normal * barycentric_coords.x + v1.normal * barycentric_coords.y + v2.normal * barycentric_coords.z, 0.0)).xyz);
    const vec2 texcoord = v0.texcoord0 * barycentric_coords.x + v1.texcoord0 * barycentric_coords.y + v2.texcoord0 * barycentric_coords.z;
    const vec3 position = (gl_ObjectToWorldEXT * vec4(v0.position * barycentric_coords.x + v1.position * barycentric_coords.y + v2.position * barycentric_coords.z, 1.0)).xyz;

    v0.position = (gl_ObjectToWorldEXT * vec4(v0.position, 1.0)).xyz;
    v1.position = (gl_ObjectToWorldEXT * vec4(v1.position, 1.0)).xyz;
    v2.position = (gl_ObjectToWorldEXT * vec4(v2.position, 1.0)).xyz;

    const vec3 hit_position = (gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT).xyz;

    vec4 material_color = vec4(1.0);

    const uint32_t material_index = mesh_description.material_index;
    const Material material = materials[material_index];

    const uint32_t entity_index = mesh_description.entity_index;
    const Object entity = entities[entity_index];
    
    material_color = material.albedo;

    if (HAS_TEXTURE(material, MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(material, MATERIAL_TEXTURE_ALBEDO_map, vec2(texcoord.x, 1.0 - texcoord.y));
        
        material_color *= albedo_texture;
    }

    float metalness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS);

    if (HAS_TEXTURE(material, MATERIAL_TEXTURE_METALNESS_MAP)) {
        float metalness_sample = SAMPLE_TEXTURE(material, MATERIAL_TEXTURE_METALNESS_MAP, vec2(texcoord.x, 1.0 - texcoord.y)).r;
        
        metalness = metalness_sample;
    }

    float roughness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_ROUGHNESS);

    if (HAS_TEXTURE(material, MATERIAL_TEXTURE_ROUGHNESS_MAP)) {
        float roughness_sample = SAMPLE_TEXTURE(material, MATERIAL_TEXTURE_ROUGHNESS_MAP, vec2(texcoord.x, 1.0 - texcoord.y)).r;
        
        roughness = roughness_sample;
    }

    const float ambient = 0.025;

    vec3 direct_lighting = vec3(0.0);
    
    const vec3 V = -gl_WorldRayDirectionEXT;//normalize(camera.position.xyz - position);
    const float NdotV = max(0.0001, dot(normal, V));

    const float material_reflectance = 0.5;
    // dialetric f0
    const float reflectance = 0.16 * material_reflectance * material_reflectance;
    vec4 F0 = vec4(material_color.rgb * metalness + (reflectance * (1.0 - metalness)), 1.0);
    vec4 F90 = vec4(clamp(dot(F0, vec4(50.0 * 0.33)), 0.0, 1.0));

    float closest_light_dist = HYP_FMATH_INFINITY;
    uint closest_light_index = ~0u;

    for (uint light_index = 0; light_index < rt_radiance_uniforms.num_bound_lights; light_index++) {
        const Light light = HYP_GET_LIGHT(light_index);

        float light_dist = CheckLightIntersection(light, hit_position, gl_WorldRayDirectionEXT);

        if (light_dist < closest_light_dist) {
            closest_light_dist = light_dist;
            closest_light_index = light_index;
        }
    }

    // russian roulette to select a light

    uint ray_seed = InitRandomSeed(InitRandomSeed(gl_LaunchIDEXT.x * 2, gl_LaunchIDEXT.x * 2 + 1), gl_PrimitiveID);

    if (closest_light_index != ~0u && RandomFloat(ray_seed) < 0.5) {
        const Light light = HYP_GET_LIGHT(closest_light_index);

        payload.distance = closest_light_dist;
        payload.emissive = vec3(UINT_TO_VEC4(light.color_encoded).rgb * light.position_intensity.w);
        payload.throughput = vec3(0.0);
        payload.color = UINT_TO_VEC4(light.color_encoded).rgb;
        payload.normal = normal;
        payload.roughness = 0.0;

        return;
    }

    payload.emissive = vec3(0.0);
    payload.throughput *= material_color.rgb;
    payload.color = material_color.rgb;

    payload.distance = gl_HitTEXT;
    payload.normal = normal;
    payload.roughness = roughness;
}
