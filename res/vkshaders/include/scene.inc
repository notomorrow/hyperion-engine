layout(std430, set = 2, binding = 0, row_major) uniform SceneDataBlock {
    mat4 view;
    mat4 projection;
    vec4 camera_position;
    vec4 light_direction;
    
    uint environment_texture_index;
    uint environment_texture_usage;
    uint resolution_x;
    uint resolution_y;
} scene;

bool HasEnvironmentTexture(uint index)
{
    return bool(scene.environment_texture_usage & (1 << index));
}