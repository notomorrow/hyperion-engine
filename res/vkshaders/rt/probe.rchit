#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require

#include "../include/defines.inc"
#include "../include/vertex.inc"
#include "../include/rt/mesh.inc"
#include "../include/rt/payload.inc"


layout(location = 0) rayPayloadInEXT RayProbePayload payload;
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

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(buffer_reference, scalar) buffer PackedVertexBuffer { PackedVertex vertices[]; };
layout(buffer_reference, scalar) buffer IndexBuffer { uint indices[]; };

layout(std140, set = 0, binding = 4) buffer MeshDescriptions
{
    MeshDescription mesh_descriptions[];
};

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
    const vec3 barycentric_coords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 normal = normalize(v0.normal * barycentric_coords.x + v1.normal * barycentric_coords.y + v2.normal * barycentric_coords.z);

    // Basic lighting
    vec3 lightVector = normalize(vec3(-0.5));
    float dot_product = max(dot(lightVector, normal), 0.6);
    
    payload.diffuse = vec3(1.0, 0.0, 0.0) * dot_product; /* TODO material albedo */
    payload.distance = gl_RayTminEXT + gl_HitTEXT;
    payload.normal = normal;
}
