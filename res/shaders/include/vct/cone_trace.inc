#ifndef CONE_TRACE_GLSL
#define CONE_TRACE_GLSL

#include "shared.inc"
#include "../noise.inc"

#define VCT_DIFFUSE_MAX_DIST          1.0
#define VCT_DIFFUSE_RATIO(roughness)  3.0
#define VCT_SPECULAR_MAX_DIST         0.8

#define VCT_SPECULAR_RATIO(roughness) RoughnessToConeAngle(roughness)

#define VCT_OFFSET_ORIGIN(normal)     vec3(normal * 0.01)

const float DIFFUSE_CONE_APERTURE_16 = 0.872665;

const vec3 DIFFUSE_CONE_DIRECTIONS_16[16] = {
    vec3( 0.57735,   0.57735,   0.57735  ),
    vec3( 0.57735,  -0.57735,  -0.57735  ),
    vec3(-0.57735,   0.57735,  -0.57735  ),
    vec3(-0.57735,  -0.57735,   0.57735  ),
    vec3(-0.903007, -0.182696, -0.388844 ),
    vec3(-0.903007,  0.182696,  0.388844 ),
    vec3( 0.903007, -0.182696,  0.388844 ),
    vec3( 0.903007,  0.182696, -0.388844 ),
    vec3(-0.388844, -0.903007, -0.182696 ),
    vec3( 0.388844, -0.903007,  0.182696 ),
    vec3( 0.388844,  0.903007, -0.182696 ),
    vec3(-0.388844,  0.903007,  0.182696 ),
    vec3(-0.182696, -0.388844, -0.903007 ),
    vec3( 0.182696,  0.388844, -0.903007 ),
    vec3(-0.182696,  0.388844,  0.903007 ),
    vec3( 0.182696, -0.388844,  0.903007 )
};

#if HYP_VCT_MODE == HYP_VCT_MODE_TEXTURE
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 30) uniform sampler3D voxel_image;

    vec4 FetchVoxel(vec3 position, float lod)
    {
        vec4 rgba = textureLod(voxel_image, clamp(position, vec3(0.0), vec3(1.0)), lod);
        rgba.rgb = pow(rgba.rgb, vec3(2.2));
        return rgba;
    }

#elif HYP_VCT_MODE == HYP_VCT_MODE_SVO

    layout(std430, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 40) readonly buffer uuOctree { uvec2 uOctree[]; };

    const float uVolumeDimension = 1024.0;
    #define VCT_ORIGIN_OFFSET 0.4

    vec4 SampleSVO(vec3 position, float lod)
    {
        uint resolution = uint(uVolumeDimension);
        uint target_resolution = uint(mix(0, 1 << 3, clamp(lod, 0.0, 7.0) / 7.0));

        uvec3 voxel_position = clamp(
            uvec3(OffsetVoxelPosition(position) * resolution), 
            uvec3(0u), 
            uvec3(resolution - 1u)
        );

        uint idx = 0, cur = 0; 
        uvec3 cmp;
        do {
            resolution >>= 1;
            cmp = uvec3(greaterThanEqual(voxel_position, uvec3(resolution)));
            idx = cur + (uint(cmp.x) | (uint(cmp.y) << 1u) | (uint(cmp.z) << 2u));
            cur = uOctree[idx].x & 0x7fffffffu;
            voxel_position -= cmp * resolution;
        } while (cur != 0u && resolution > target_resolution);

        return RGBA8ToVec4Raw(uOctree[idx].y) / 255.0;
    }

    vec4 FetchVoxel(vec3 position, float lod)
    {
        float lower_level = float(floor(lod));
        float upper_level = float(ceil(lod));

        vec4 lower_sample = SampleSVO(position, lower_level);
        vec4 upper_sample = SampleSVO(position, upper_level);

        return mix(lower_sample, upper_sample, fract(lod));
    }
#endif

vec4 ConeTrace(float min_diameter, vec3 origin, vec3 dir, float ratio, float max_dist, bool include_lighting)
{
    const float min_diameter_inv = 1.0 / min_diameter;

    vec4 accum = vec4(0.0);
    vec3 sample_pos = origin;
    float dist = 0.0;
    float diameter = max(min_diameter, dist * ratio);

    while (dist < max_dist && accum.a < 1.0) {
        float lod = log2(diameter * min_diameter_inv);

        sample_pos = origin + dir * dist;

        vec4 voxel_color = FetchVoxel(sample_pos, lod);
        voxel_color.rgb *= mix(1.0, voxel_color.a, float(include_lighting));
        voxel_color.rgb *= 1.0 - clamp(dist / max_dist, 0.0, 1.0);

        float weight = (1.0 - accum.a);
        accum += voxel_color * weight;

        const float prev_dist = dist;
        dist += max(diameter, min_diameter);
        diameter = dist * ratio;
    }

    return accum;
}

const float diffuse_aperture = 2.0 * tan(DIFFUSE_CONE_APERTURE_16 * 0.5);

vec4 GetVoxelColorAtPosition(vec3 P)
{
    return FetchVoxel(VctWorldToTexCoord(P), 0.0);
}

vec4 ConeTraceDiffuse(vec3 P, vec3 N, vec3 T, vec3 B, float roughness)
{
    const vec3 normal_offset = VCT_OFFSET_ORIGIN(N);

    const vec3 voxel_coord = VctWorldToTexCoord(P);
    
    const float greatest_extent = vct_greatest_extent;

    vec4 result = vec4(0.0);

    uvec2 pixel_coord = uvec2(v_texcoord0 * vec2(camera.dimensions.xy) - 0.5);
    uint _seed = InitRandomSeed(InitRandomSeed(pixel_coord.x, pixel_coord.y), scene.frame_counter % 256);

    result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, N, VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST, false);
    result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, normalize(N + T), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST, false) * 0.707;
    result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, normalize(N - T), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST, false) * 0.707;
    result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, normalize(N + B), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST, false) * 0.707;
    result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, normalize(N - B), VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST, false) * 0.707;

    return result / 4.0;

    // for (int i = 0; i < 16; i++) {
    //     vec3 dir = RandomInHemisphere(vec3(RandomFloat(_seed), RandomFloat(_seed), RandomFloat(_seed)), N);
    //     float cos_theta = dot(N, dir);
    //     result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, dir, VCT_DIFFUSE_RATIO(roughness), VCT_DIFFUSE_MAX_DIST) * cos_theta;
    // }   

    // return result / 16.0; 


    // for (int i = 0; i < 16; i++) {
    //     float cos_theta = dot(N, DIFFUSE_CONE_DIRECTIONS_16[i]);

    //     if (cos_theta > 0.0) {
    //         result += ConeTrace(1.0 / greatest_extent, voxel_coord + normal_offset, DIFFUSE_CONE_DIRECTIONS_16[i], diffuse_aperture, VCT_DIFFUSE_MAX_DIST) * cos_theta;
    //     }
    // }

    // return result / 16.0;
}

vec4 ConeTraceSpecular(vec3 P, vec3 N, vec3 R, float roughness)
{
    // if (roughness >= 1.0) {
    //     return vec4(0.0);
    // }

    const vec3 voxel_coord = VctWorldToTexCoord(P);
    
    const float greatest_extent = vct_greatest_extent;

    vec4 result = vec4(0.0);

    result += ConeTrace(1.0 / greatest_extent, voxel_coord + VCT_OFFSET_ORIGIN(N), normalize(R), VCT_SPECULAR_RATIO(roughness), VCT_SPECULAR_MAX_DIST, true);

    return result;
}

#endif
