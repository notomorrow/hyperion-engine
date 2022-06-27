#ifndef CONE_TRACE_GLSL
#define CONE_TRACE_GLSL

#include "shared.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 25) uniform sampler3D voxel_image;

#define VCT_DIFFUSE_MAX_DIST          0.3
#define VCT_DIFFUSE_RATIO_MIN         1.0
#define VCT_DIFFUSE_RATIO_MULTIPLIER  2.0
#define VCT_DIFFUSE_RATIO(roughness)  ((roughness) * VCT_DIFFUSE_RATIO_MULTIPLIER + VCT_DIFFUSE_RATIO_MIN)

#define VCT_SPECULAR_MAX_DIST         0.3
#define VCT_SPECULAR_RATIO_MIN        0.1
#define VCT_SPECULAR_RATIO_MULTIPLIER 1.0
#define VCT_SPECULAR_RATIO(roughness) ((roughness) * VCT_SPECULAR_RATIO_MULTIPLIER + VCT_SPECULAR_RATIO_MIN)

#define VCT_OFFSET_ORIGIN(normal)     (normalize(normal) * 0.7)

vec4 FetchVoxel(vec3 position, float lod)
{
    return textureLod(voxel_image, position, lod);
}

vec4 ConeTrace(float min_diameter, vec3 origin, vec3 dir, float ratio, float max_dist)
{
    const float min_diameter_inv = 1.0 / min_diameter;

    vec4 accum      = vec4(0.0);
    vec4 fade       = vec4(0.0);
    vec3 sample_pos = origin;
    float dist      = min_diameter;

    while (dist < max_dist) {
        float diameter = max(min_diameter, ratio * dist);
        float lod      = log2(diameter * min_diameter_inv);

        sample_pos = origin + dir * dist;

        vec4 voxel_color = FetchVoxel(sample_pos, lod);
        voxel_color *= 1.0 - clamp(dist / max_dist, 0.0, 1.0);

        float weight = (1.0 - accum.a);
        accum += voxel_color * weight;
        dist += diameter;
    }

    return accum;
}

vec4 ConeTraceDiffuse(vec3 P, vec3 N, vec3 tangent, vec3 bitangent, float roughness)
{
    const vec3 storage_coord    = VctStoragePosition(VctWorldToAabb(P + VCT_OFFSET_ORIGIN(N)));
    const vec3  voxel_coord     = vec3(storage_coord) / vec3(vct_params.extent.xyz);
    const float greatest_extent = float(vct_greatest_extent);

    vec4 result = vec4(0.0);

    result += ConeTrace(1.0 / greatest_extent, voxel_coord, N, VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST);

    return result;
}

vec4 ConeTraceSpecular(vec3 P, vec3 N, vec3 R, float roughness)
{
    const vec3 storage_coord    = VctStoragePosition(VctWorldToAabb(P + VCT_OFFSET_ORIGIN(N)));
    const vec3  voxel_coord     = vec3(storage_coord) / vec3(vct_params.extent.xyz);
    const float greatest_extent = float(vct_greatest_extent);

    vec4 result = vec4(0.0);

    result += ConeTrace(1.0 / greatest_extent, voxel_coord, normalize(R), VCT_SPECULAR_RATIO(roughness), VCT_SPECULAR_MAX_DIST);

    return result;
}

#endif
