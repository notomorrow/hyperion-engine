#ifndef ENV_GRID_RADIANCE_GLSL
#define ENV_GRID_RADIANCE_GLSL



vec4 FetchVoxel(vec3 position, float lod)
{
    vec4 rgba = textureLod(sampler3D(voxel_image, sampler_linear), position, lod);
    rgba.rgb = ReverseTonemapReinhardSimple(rgba.rgb);
    return rgba;
}

vec3 VctWorldToTexCoord(vec3 world_position)
{
    const vec3 voxel_grid_aabb_min = vec3(min(env_grid.aabb_min.x, min(env_grid.aabb_min.y, env_grid.aabb_min.z)));
    const vec3 voxel_grid_aabb_max = vec3(max(env_grid.aabb_max.x, max(env_grid.aabb_max.y, env_grid.aabb_max.z)));
    const vec3 voxel_grid_aabb_extent = voxel_grid_aabb_max - voxel_grid_aabb_min;
    const vec3 voxel_grid_aabb_center = voxel_grid_aabb_min + voxel_grid_aabb_extent * 0.5;

    const vec3 scaled_position = (world_position - voxel_grid_aabb_center) / voxel_grid_aabb_extent;
    const vec3 voxel_storage_position = (scaled_position * 0.5 + 0.5);

    return voxel_storage_position;
}

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

vec4 ConeTraceSpecular(vec3 P, vec3 N, vec3 R, float roughness)
{
    // if (roughness >= 1.0) {
    //     return vec4(0.0);
    // }

    const vec3 voxel_coord = VctWorldToTexCoord(P);
    const float greatest_extent = 256.0;
    const float voxel_size = 1.0 / greatest_extent;

    return ConeTrace(voxel_size, voxel_coord + N * max(0.01, voxel_size), R, RoughnessToConeAngle(roughness), 0.65, false);
}

vec4 ComputeVoxelRadiance(vec3 world_position, vec3 N, vec3 V, float roughness, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
{
    const vec3 R = normalize(reflect(-V, N));

    return ConeTraceSpecular(world_position, N, R, roughness);
}

#endif // ENV_GRID_RADIANCE_GLSL