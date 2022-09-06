#ifndef CONE_TRACE_GLSL
#define CONE_TRACE_GLSL

#include "shared.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 30) uniform sampler3D voxel_image;

#define VCT_DIFFUSE_MAX_DIST          0.1
#define VCT_DIFFUSE_RATIO(roughness)  RoughnessToConeAngle(roughness)
#define VCT_SPECULAR_MAX_DIST         0.12

#define VCT_SPECULAR_RATIO(roughness) RoughnessToConeAngle(0.015)

#define VCT_OFFSET_ORIGIN(normal)     vec3(normal * 0.015)

vec4 FetchVoxel(vec3 position, float lod)
{
    vec4 rgba = textureLod(voxel_image, clamp(position, vec3(0.0), vec3(1.0)), lod);
    rgba.rgb = pow(rgba.rgb, vec3(2.2));
    return rgba;
}

vec4 ConeTrace(float min_diameter, vec3 origin, vec3 dir, float ratio, float max_dist)
{
    const float min_diameter_inv = 1.0 / min_diameter;

    vec4 accum = vec4(0.0);
    vec3 sample_pos = origin;
    float dist = min_diameter;

    while (dist < max_dist && accum.a < 1.0) {
        float diameter = max(min_diameter, ratio * dist);
        float lod = log2(diameter * min_diameter_inv);

        sample_pos = origin + dir * dist;

        vec4 voxel_color = FetchVoxel(sample_pos, lod);
        voxel_color.rgb *= 1.0 - clamp(dist / max_dist, 0.0, 1.0);

        float weight = (1.0 - accum.a);
        accum += voxel_color * weight;
        dist += diameter;
    }

    return accum;
}

vec4 ConeTraceDiffuse(vec3 P, vec3 N, vec3 T, vec3 B, float roughness)
{
    const vec3 normal_offset = VCT_OFFSET_ORIGIN(N);

    const vec3 storage_coord = VctStoragePosition(VctWorldToAABB(P));
    const vec3 voxel_coord = vec3(storage_coord) / vec3(vct_params.extent.xyz - vec3(1.0));
    const float greatest_extent = vct_greatest_extent;

    vec4 result = vec4(0.0);

    result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, N, VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST);
    result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, normalize(N + T), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST) * 0.707;
    result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, normalize(N - T), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST) * 0.707;
    result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, normalize(N + B), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST) * 0.707;
    result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, normalize(N - B), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST) * 0.707;

    return result;
}

vec4 ConeTraceSpecular(vec3 P, vec3 N, vec3 R, float roughness)
{
    if (roughness >= 1.0) {
        return vec4(0.0);
    }

    const vec3 storage_coord = VctStoragePosition(VctWorldToAABB(P));
    const vec3 voxel_coord = vec3(storage_coord) / vec3(vct_params.extent.xyz - vec3(1.0));
    const float greatest_extent = vct_greatest_extent;

    vec4 result = vec4(0.0);

    result += ConeTrace(1.0 / greatest_extent, voxel_coord + VCT_OFFSET_ORIGIN(N), normalize(R), VCT_SPECULAR_RATIO(roughness), VCT_SPECULAR_MAX_DIST);

    return result;
}

#endif
