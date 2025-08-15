#ifndef HYP_RT_RADIANCE_GLSL
#define HYP_RT_RADIANCE_GLSL

struct RTRadianceUniforms
{
    uint num_bound_lights;
    uint ray_offset; // for lightmapper
    float min_roughness;
    float _pad0;

    ivec2 output_image_resolution;

    float _pad1;
    float _pad2;
    uvec4 light_indices[4];
};

#define HYP_GET_LIGHT(index) \
    lights[rt_radiance_uniforms.light_indices[(index / 4)][index % 4]]

#endif