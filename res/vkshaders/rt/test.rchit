#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

struct RayPayload {
	vec3 color;
	float distance;
	vec3 normal;
	float reflector;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

hitAttributeEXT vec2 attribs;

struct PackedVertex {
    float position_x;
    float position_y;
    float position_z;
    float normal_x;
    float normal_y;
    float normal_z;
    float texcoord_s;
    float texcoord_t;
};

layout(set = 9, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(std430, set = 9, binding = 3) buffer Vertices {
    PackedVertex vertices[];
};

layout(set = 9, binding = 4) buffer Indices {
    uint indices[];
};

#include "../include/scene.inc"
#include "../include/vertex.inc"

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
    ivec3 index = ivec3(
        indices[3 * gl_PrimitiveID],
        indices[3 * gl_PrimitiveID + 1],
        indices[3 * gl_PrimitiveID + 2]
    );

    Vertex v0 = UnpackVertex(vertices[index.x]);
    Vertex v1 = UnpackVertex(vertices[index.y]);
    Vertex v2 = UnpackVertex(vertices[index.z]);

    // Interpolate normal
    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 normal = normalize(v0.normal * barycentricCoords.x + v1.normal * barycentricCoords.y + v2.normal * barycentricCoords.z);

    // Basic lighting
    vec3 lightVector = normalize(scene.light_direction.xyz);
    float dot_product = max(dot(lightVector, normal), 0.6);
    rayPayload.color = vec3(1.0, 0.0, 0.0) * vec3(dot_product);
    rayPayload.distance = gl_RayTmaxEXT;
    rayPayload.normal = normal;

    // Objects with full white vertex color are treated as reflectors
    rayPayload.reflector = 0.2f; 
}
