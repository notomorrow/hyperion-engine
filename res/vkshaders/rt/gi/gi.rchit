#version 460
#extension GL_EXT_ray_tracing          : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_buffer_reference     : require

#define HYP_NO_CUBEMAP

#include "../../include/defines.inc"
#include "../../include/vertex.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/material.inc"
#include "../../include/object.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/mesh.inc"
#include "../../include/rt/payload.inc"

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

layout(set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(buffer_reference, scalar) readonly buffer PackedVertexBuffer { float vertices[]; };
layout(buffer_reference, scalar) readonly buffer IndexBuffer { uvec3 indices[]; };

layout(std140, set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 4) buffer MeshDescriptions
{
    MeshDescription mesh_descriptions[];
};

layout(std140, set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 5) readonly buffer MaterialBuffer
{
    Material materials[];
};

layout(std140, set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 6) readonly buffer EntityBuffer
{
    Object entities[];
};

// for RT, all textures are bindless
layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY) uniform sampler2D textures[];

// Vertex UnpackVertex(uint index)
// {
//     Vertex vertex;
    
//     vertex.position = vec3(
//         vertices[index],
//         vertices[index + 1],
//         vertices[index + 2]
//     );
    
//     vertex.normal = vec3(
//         vertices[index + 3],
//         vertices[index + 4],
//         vertices[index + 5]
//     );
    
//     vertex.texcoord0 = vec2(
//         vertices[index + 6],
//         vertices[index + 7]
//     );
    
//     return vertex;
// }

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

    v0.position = (gl_ObjectToWorldEXT * vec4(v0.position, 1.0)).xyz;
    v1.position = (gl_ObjectToWorldEXT * vec4(v1.position, 1.0)).xyz;
    v2.position = (gl_ObjectToWorldEXT * vec4(v2.position, 1.0)).xyz;

    // Interpolate normal
    const vec3 barycentric_coords = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    const vec3 normal = normalize((gl_ObjectToWorldEXT * vec4(v0.normal * barycentric_coords.x + v1.normal * barycentric_coords.y + v2.normal * barycentric_coords.z, 0.0)).xyz);
    const vec2 texcoord = v0.texcoord0 * barycentric_coords.x + v1.texcoord0 * barycentric_coords.y + v2.texcoord0 * barycentric_coords.z;
    
    vec4 material_color = vec4(1.0);

    const uint32_t material_index = mesh_description.material_index;
    const Material material = materials[material_index];

    const uint32_t entity_index = mesh_description.entity_index;
    const Object entity = entities[entity_index];
    
    material_color = material.albedo;

    if (HAS_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map, texcoord);
        
        material_color *= albedo_texture;
    }

    payload.color = material_color.rgb;
    payload.distance = gl_RayTminEXT + gl_HitTEXT;
    payload.normal = normal;
    payload.roughness = GET_MATERIAL_PARAM(MATERIAL_PARAM_ROUGHNESS);
}
