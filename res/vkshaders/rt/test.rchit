#version 460
#extension GL_EXT_ray_tracing          : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_buffer_reference2    : require

#define HYP_NO_CUBEMAP

#include "../include/defines.inc"
#include "../include/vertex.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/material.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/rt/mesh.inc"
#include "../include/rt/payload.inc"

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

layout(buffer_reference, std140, scalar, buffer_reference_align = 16) buffer PackedVertexBuffer { PackedVertex vertices[]; };
layout(buffer_reference, std140, scalar, buffer_reference_align = 4) buffer IndexBuffer { uint indices[]; };

layout(std140, set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 4) buffer MeshDescriptions
{
    MeshDescription mesh_descriptions[];
};

layout(std140, set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 5) readonly buffer MaterialBuffer
{
    Material materials[];
};

// for RT, all textures are bindless
layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY) uniform sampler2D textures[];

Vertex UnpackVertex(PackedVertex packed_vertex)
{
    Vertex vertex;
    
    vertex.position = vec3(
        packed_vertex.position_x,
        packed_vertex.position_y,
        packed_vertex.position_z
    );
    
    vertex.normal = vec3(
        packed_vertex.normal_x,
        packed_vertex.normal_y,
        packed_vertex.normal_z
    );
    
    vertex.texcoord0 = vec2(
        packed_vertex.texcoord_s,
        packed_vertex.texcoord_t
    );
    
    return vertex;
}

void main()
{
    MeshDescription mesh_description = mesh_descriptions[gl_InstanceCustomIndexEXT];
    
    PackedVertexBuffer vertex_buffer = PackedVertexBuffer(mesh_description.vertex_buffer_address);
    IndexBuffer index_buffer = IndexBuffer(mesh_description.index_buffer_address);
    
    ivec3 index = ivec3(
        index_buffer.indices[3 * gl_PrimitiveID],
        index_buffer.indices[3 * gl_PrimitiveID + 1],
        index_buffer.indices[3 * gl_PrimitiveID + 2]
    );

    Vertex v0 = UnpackVertex(vertex_buffer.vertices[index.x]);
    Vertex v1 = UnpackVertex(vertex_buffer.vertices[index.y]);
    Vertex v2 = UnpackVertex(vertex_buffer.vertices[index.z]);

    // Interpolate normal
    const vec3 barycentric_coords = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    const vec3 normal = normalize(v0.normal * barycentric_coords.x + v1.normal * barycentric_coords.y + v2.normal * barycentric_coords.z);
    const vec2 texcoord = v0.texcoord0 * barycentric_coords.x + v1.texcoord0 * barycentric_coords.y + v2.texcoord0 * barycentric_coords.z;

    // Basic lighting
    vec3 lightVector = normalize(vec3(-0.5));
    float dot_product = max(dot(lightVector, normal), 0.6);

    const uint32_t material_index = mesh_description.material_index;
    const Material material = materials[material_index];
    
    vec4 material_color = material.albedo;

#if 1
    if (HAS_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map, texcoord);
        
        material_color *= albedo_texture;
    }
#endif

    payload.color = material_color.rgb;
    payload.distance = gl_HitTEXT;
    payload.normal = normal;
    payload.roughness = GET_MATERIAL_PARAM(MATERIAL_PARAM_ROUGHNESS);
}
