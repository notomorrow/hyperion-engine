
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

#define MATERIAL_TEXTURE_ALBEDO_map    0
#define MATERIAL_TEXTURE_NORMAL_MAP    1
#define MATERIAL_TEXTURE_AO_MAP        2
#define MATERIAL_TEXTURE_PARALLAX_MAP  3
#define MATERIAL_TEXTURE_METALNESS_MAP 4
#define MATERIAL_TEXTURE_ROUGHNESS_MAP 5
#define MATERIAL_TEXTURE_SKYBOX_MAP    6
#define MATERIAL_TEXTURE_COLOR_MAP     7
#define MATERIAL_TEXTURE_POSITION_MAP  8
#define MATERIAL_TEXTURE_DATA_MAP      9
#define MATERIAL_TEXTURE_SSAO_MAP      10
#define MATERIAL_TEXTURE_TANGENT_MAP   11
#define MATERIAL_TEXTURE_BITANGENT_MAP 12
#define MATERIAL_TEXTURE_DEPTH_MAP     13

bool HasMaterialTexture(uint index)
{
    return bool(material.texture_usage & (1 << index));
}