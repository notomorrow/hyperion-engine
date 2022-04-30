
struct Material {
    vec4 albedo;
    
    float metalness;
    float roughness;
    float subsurface;
    float specular;
    
    float specular_tint;
    float anisotropic;
    float sheen;
    float sheen_tint;
    
    float clearcoat;
    float clearcoat_gloss;
    float emissiveness;
    float _padding0;
    
    uint uv_flip_s;
    uint uv_flip_t;
    float uv_scale;
    float parallax_height;
    
    uint texture_index[32];
    uint texture_usage;
    uint _padding1;
    uint _padding2;
    
    /* Texture schema:
       0 - albedo
       1 - normals
       2 - ao
       3 - parallax
       4 - metalness
       5 - roughness
       6 - envmap */
};

layout(std430, set = 3, binding = 0) readonly buffer MaterialBuffer {
    Material material;
};

bool HasMaterialTexture(uint index)
{
    return bool(material.texture_usage & (1 << index));
}