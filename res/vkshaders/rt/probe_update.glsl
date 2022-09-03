#extension GL_EXT_scalar_block_layout : require

#define CACHE_SIZE 64
#define EPS 0.00001
#define ENERGY_CONSERVATION 0.95
#define MAX_DISTANCE (probe_system.probe_distance * 1.5)
#define SQR(x) (x * x)
#define PROBE_TEXEL_FETCH(coord) (GetProbeRayData(coord / probe_system.image_dimensions))

#if DEPTH
    #define GROUP_SIZE 16
#else
    #define GROUP_SIZE 8
#endif

layout(local_size_x = GROUP_SIZE, local_size_y = GROUP_SIZE, local_size_z = 1) in;

#include "../include/rt/probe/shared.inc"

#if DEPTH
    #define PROBE_SIDE_LENGTH PROBE_SIDE_LENGTH_DEPTH
    #define OUTPUT_IMAGE output_depth
#else
    #define PROBE_SIDE_LENGTH PROBE_SIDE_LENGTH_IRRADIANCE
    #define OUTPUT_IMAGE output_irradiance
#endif

#define PROBE_SIDE_LENGTH_BORDER (PROBE_SIDE_LENGTH + probe_system.probe_border.x)

shared ProbeRayData ray_cache[CACHE_SIZE];

layout(set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 11, rgba16f) uniform image2D output_irradiance;
layout(set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 12, rg16f)   uniform image2D output_depth;

vec2 NormalizeOctahedralCoord(uvec2 coord)
{
    vec2 oct_frag_coord = ivec2((int(coord.x) - 2) % PROBE_SIDE_LENGTH_BORDER, (int(coord.y) - 2) % PROBE_SIDE_LENGTH_BORDER);
    
    return (oct_frag_coord + vec2(0.5)) * (2.0 / float(PROBE_SIDE_LENGTH)) - vec2(1.0);
}

void UpdateRayCache(uvec2 coord, uint offset, uint num_rays)
{
    const uint index = uint(gl_LocalInvocationIndex);
    
    if (index >= num_rays) {
        return;
    }
    
    
    uint probes_per_side = (probe_system.irradiance_image_dimensions.x - 2) / PROBE_SIDE_LENGTH_BORDER;
    uint probe_index = uint(coord.x / PROBE_SIDE_LENGTH_BORDER) + probes_per_side * uint(coord.y / PROBE_SIDE_LENGTH_BORDER);
    
    ray_cache[index] = GetProbeRayData(uvec2(probe_index, offset + index));
    
    //PROBE_TEXEL_FETCH(uvec2(PROBE_RAY_DATA_INDEX(coord), offset + index));
}

void GatherRays(uvec2 coord, uint num_rays, inout vec3 result, inout float total_weight)
{
    ProbeRayData ray;
    
    for (uint i = 0; i < num_rays; i++) {
        ray                = ray_cache[i];
        vec3 ray_direction = ray.direction_depth.xyz;
        vec3 ray_origin    = ray.origin.xyz;
        float ray_depth    = ray.direction_depth.w;
        vec4 radiance;
        
#if DEPTH
        float dist = min(MAX_DISTANCE, ray_depth - 0.01);
        
        if (dist == -1.0) {
            dist = MAX_DISTANCE;
        }
#else
        radiance = ray.color;
        radiance.rgb *= ENERGY_CONSERVATION;
#endif


        vec3 texel_direction = DecodeOctahedralCoord(NormalizeOctahedralCoord(coord));
        
        float weight = max(0.0, dot(texel_direction, ray_direction));

#if DEPTH
        weight = pow(weight, 1.0 /* depth sharpness */);
#endif

        if (weight >= EPS) {
#if DEPTH
            result += vec3(dist * weight, SQR(dist) * weight, 0.0);
#else
            result += vec3(radiance.rgb * weight);
#endif

            total_weight += weight;
        }
    }
}

void main()
{
    const uvec2 coord = uvec2(gl_GlobalInvocationID.xy) + (uvec2(gl_WorkGroupID.xy) * uvec2(2)) + uvec2(2);
    
    vec3  result       = vec3(0.0);
    float total_weight = 0.0;

    uint remaining_rays = probe_system.num_rays_per_probe;
    uint offset = 0;

    while (remaining_rays > 0) {
        uint num_rays = min(CACHE_SIZE, remaining_rays);
        
        UpdateRayCache(coord, offset, num_rays);

        barrier();

        GatherRays(coord, num_rays, result, total_weight);

        barrier();

        remaining_rays -= num_rays;
        offset += num_rays;
    }
    
    if (total_weight > EPS) {
        result /= total_weight;
    }
    
    imageStore(
        OUTPUT_IMAGE,
        ivec2(coord),
        vec4(result.rgb, 1.0)
    );
}
