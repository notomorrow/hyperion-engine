layout(std430, set = 2, binding = 0, row_major) readonly buffer SceneDataBlock {
    mat4 view;
    mat4 projection;
    vec4 camera_position;
    vec4 camera_direction;
    vec4 light_direction;
    
    uint environment_texture_index;
    uint environment_texture_usage;
    uint resolution_x;
    uint resolution_y;
    
    vec4 aabb_max;
    vec4 aabb_min;
} scene;

struct Light {
    vec4 position; // or direction for directional lights
    uint color_encoded;
    uint type;
    float intensity;
    float _pad0;
};

layout(std430, set = 2, binding = 1) readonly buffer LightDataBlock {
    Light lights[];
};

bool HasEnvironmentTexture(uint index)
{
    return bool(scene.environment_texture_usage & (1 << index));
}