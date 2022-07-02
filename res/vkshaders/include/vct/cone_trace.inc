#ifndef CONE_TRACE_GLSL
#define CONE_TRACE_GLSL

#include "shared.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 30) uniform sampler3D voxel_image;


#define VCT_DIFFUSE_MAX_DIST          0.3
#define VCT_DIFFUSE_RATIO(roughness)  (mix(8.0, 20.0, sqrt(roughness)))
#define VCT_SPECULAR_MAX_DIST         0.5

// rough 0 = 0.025
// rough 1.0 = 
#define VCT_SPECULAR_RATIO(roughness) (mix(0.015, 1.0, sqrt(roughness)))

#define VCT_OFFSET_ORIGIN(normal)     (normalize(normal) * 1.05)

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

    const float max_lod = 6;

    while (dist < max_dist && accum.a < 1.0) {
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

vec4 ConeTraceDiffuse(vec3 P, vec3 N, vec3 T, vec3 B, float roughness)
{
    const vec3 storage_coord    = VctStoragePosition(VctWorldToAabb(P + VCT_OFFSET_ORIGIN(N)));
    const vec3  voxel_coord     = vec3(storage_coord) / vec3(vct_params.extent.xyz);
    const float greatest_extent = float(vct_greatest_extent);

    vec4 result = vec4(0.0);

    result += ConeTrace(1.0 / greatest_extent, voxel_coord, N, VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST);
    result += ConeTrace(1.0 / greatest_extent, voxel_coord, normalize(N + T), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST);// * 0.707;
    result += ConeTrace(1.0 / greatest_extent, voxel_coord, normalize(N - T), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST);// * 0.707;
    result += ConeTrace(1.0 / greatest_extent, voxel_coord, normalize(N + B), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST);// * 0.707;
    result += ConeTrace(1.0 / greatest_extent, voxel_coord, normalize(N - B), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST);// * 0.707;

    return result / 5.0;
}

vec4 ConeTraceSpecular(vec3 P, vec3 N, vec3 R, float roughness)
{
    if (roughness >= 1.0) {
        return vec4(0.0);
    }

    const vec3 storage_coord    = VctStoragePosition(VctWorldToAabb(P + VCT_OFFSET_ORIGIN(N)));
    const vec3  voxel_coord     = vec3(storage_coord) / vec3(vct_params.extent.xyz);
    const float greatest_extent = float(vct_greatest_extent);

    vec4 result = vec4(0.0);

    result += ConeTrace(1.0 / greatest_extent, voxel_coord, normalize(R), VCT_SPECULAR_RATIO(roughness), VCT_SPECULAR_MAX_DIST);

    return result * HYP_FMATH_SQR(1.0 - roughness);
}

#endif
