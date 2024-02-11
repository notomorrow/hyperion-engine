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

#include "../../include/brdf.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/mesh.inc"

#define PATHTRACER
#include "../../include/rt/payload.inc"

layout(set = 0, binding = 9) uniform sampler sampler_nearest;
#define HYP_SAMPLER_NEAREST sampler_nearest

layout(set = 0, binding = 10) uniform sampler sampler_linear;
#define HYP_SAMPLER_LINEAR sampler_linear


/* Shadows */

layout(set = 1, binding = 12) uniform texture2D shadow_maps[];

layout(std140, set = 1, binding = 13, row_major) readonly buffer ShadowShaderData
{
    ShadowMap shadow_map_data[HYP_MAX_SHADOW_MAPS];
};

layout(std140, set = 1, binding = 4, row_major) uniform CameraShaderData
{
    Camera camera;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../include/shadows.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../../include/rt/RTRadiance.inc"

/* End Shadows */

#undef HYP_NO_CUBEMAP

layout(location = 0) rayPayloadInEXT RayPayload payload;
hitAttributeEXT vec2 attribs;

#define HYP_GET_LIGHT(index) \
    lights[rt_radiance_uniforms.light_indices[(index / 4)][index % 4]]

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

layout(buffer_reference, scalar) readonly buffer PackedVertexBuffer { float vertices[]; };
layout(buffer_reference, scalar) readonly buffer IndexBuffer { uvec3 indices[]; };

layout(std140, set = 0, binding = 2) buffer MeshDescriptions
{
    MeshDescription mesh_descriptions[];
};

layout(std140, set = 0, binding = 3) readonly buffer MaterialBuffer
{
    Material materials[];
};

layout(std140, set = 0, binding = 4) readonly buffer EntityBuffer
{
    Object entities[];
};

layout(std430, set = 0, binding = 5) readonly buffer LightShaderData
{
    Light lights[];
};

layout(std140, set = 0, binding = 12, row_major) uniform RTRadianceUniformBuffer
{
    RTRadianceUniforms rt_radiance_uniforms;
};

// for RT, all textures are bindless
layout(set = 2, binding = 0) uniform sampler2D textures[];

void CheckLightIntersection(const Light light, const vec3 position, const vec3 normal, const vec3 V, inout vec3 direct_lighting) {
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

    for (uint light_index = 0; light_index < rt_radiance_uniforms.num_bound_lights; light_index++) {
        const Light light = HYP_GET_LIGHT(light_index);

        const vec3 L = CalculateLightDirection(light, position);
        const vec3 H = normalize(L + V);

        const float NdotL = max(0.0001, dot(normal, L));
        const float NdotH = max(0.0001, dot(normal, H));
        const float LdotH = max(0.0001, dot(L, H));
        const float HdotV = max(0.0001, dot(H, V));
        
        const float D = CalculateDistributionTerm(roughness, NdotH);
        const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
        const vec4 F = CalculateFresnelTerm(F0, roughness, LdotH);
        
        const vec4 dfg = CalculateDFG(F, roughness, NdotV);
        const vec4 E = CalculateE(F0, dfg);
        const vec3 energy_compensation = CalculateEnergyCompensation(F0.rgb, dfg.rgb);
        
        const vec4 diffuse_color = CalculateDiffuseColor(material_color, metalness);
        const vec4 specular_lobe = D * G * F;

        vec4 diffuse_lobe = diffuse_color * (1.0 / HYP_FMATH_PI);
        
        const float attenuation = light.type == HYP_LIGHT_TYPE_POINT
            ? GetSquareFalloffAttenuation(position.xyz, light.position_intensity.xyz, light.radius)
            : 1.0;

        //vec3 local_light = vec3(NdotL) * UINT_TO_VEC4(light.color_encoded).rgb;

        //local_light *= light.position_intensity.w * attenuation;

        //if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && light.shadow_map_index != ~0u) {
        //    local_light *= GetShadowStandard(light.shadow_map_index, position.xyz, vec2(0.0), NdotL);
        //}

        //direct_lighting += material_color.rgb * local_light;
        
        const vec4 direct_component = diffuse_lobe + specular_lobe;
        direct_lighting += (direct_component.rgb * NdotL * attenuation);
    }


    //payload.beta *= exp(-payload.absorption * gl_HitTEXT);
    payload.color = direct_lighting;// * payload.beta;
    //payload.beta *= F.rgb * NdotL / ((NdotL * (1.0 / HYP_FMATH_PI)) + HYP_FMATH_EPSILON);


    payload.distance = gl_HitTEXT;
    payload.normal = normal;
    payload.roughness = roughness;
}
