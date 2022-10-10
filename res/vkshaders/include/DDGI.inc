#ifndef HYP_DDGI_GLSL
#define HYP_DDGI_GLSL

#include "rt/probe/shared.inc"
layout(set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 13) uniform texture2D probe_irradiance;
layout(set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 14) uniform texture2D probe_depth;

vec4 DDGISampleIrradiance(vec3 P, vec3 N, vec3 V)
{
    const uvec3 base_grid_coord    = BaseGridCoord(P);
    const vec3 base_probe_position = GridPositionToWorldPosition(base_grid_coord);
    
    vec3 total_irradiance = vec3(0.0);
    float total_weight    = 0.0;
    
    vec3 alpha = clamp((P - base_probe_position) / PROBE_GRID_STEP, vec3(0.0), vec3(1.0));
    
    for (uint i = 0; i < 8; i++) {
        uvec3 offset = uvec3(i, i >> 1, i >> 2) & uvec3(1);
        uvec3 probe_grid_coord = clamp(base_grid_coord + offset, uvec3(0.0), probe_system.probe_counts.xyz - uvec3(1));
        
        uint probe_index    = GridPositionToProbeIndex(probe_grid_coord);
        vec3 probe_position = GridPositionToWorldPosition(probe_grid_coord);
        vec3 probe_to_point = P - probe_position + (N + 3.0 * V) * PROBE_NORMAL_BIAS;
        vec3 dir            = normalize(-probe_to_point);
        
        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight   = 1.0;
        
        /* Backface test */
        
        vec3 true_direction_to_probe = normalize(probe_position - P);
        weight *= HYP_FMATH_SQR(max(0.0001, (dot(true_direction_to_probe, N) + 1.0) * 0.5)) + 0.2;
        
        /* Visibility test */
        vec2 depth_texcoord = TextureCoordFromDirection(-dir, probe_index, probe_system.depth_image_dimensions, PROBE_SIDE_LENGTH_DEPTH);
        float distance_to_probe = length(probe_to_point);

        vec2 depth_sample = Texture2DLod(gbuffer_depth_sampler, probe_depth, depth_texcoord, 0.0).rg;
        float mean = depth_sample.x;
        float variance = abs(HYP_FMATH_SQR(mean) - depth_sample.y);

        float chebyshev = variance / (variance + HYP_FMATH_SQR(max(distance_to_probe - mean, 0.0)));
        chebyshev = max(HYP_FMATH_CUBE(chebyshev), 0.0);
        weight *= (distance_to_probe <= mean) ? 1.0 : chebyshev;
        
        weight = max(0.000001, weight);

        vec3 irradiance_dir = N;
        vec2 irradiance_texcoord = TextureCoordFromDirection(normalize(irradiance_dir), probe_index, probe_system.irradiance_image_dimensions, PROBE_SIDE_LENGTH_IRRADIANCE);
        vec3 irradiance = Texture2DLod(gbuffer_depth_sampler, probe_irradiance, irradiance_texcoord, 0.0).rgb;

        const float crush_threshold = 0.2;
        if (weight < crush_threshold) {
            weight *= weight * weight * (1.0 / HYP_FMATH_SQR(crush_threshold));
        }

        // trilinear
        weight *= trilinear.x * trilinear.y * trilinear.z;

        total_irradiance += irradiance * weight;
        total_weight += weight;
    }

    vec3 net_irradiance = total_irradiance / total_weight;

    return vec4(net_irradiance, 1.0);
}

#endif