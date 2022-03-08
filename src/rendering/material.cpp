#include "material.h"

namespace hyperion {

const Material::MaterialParameterTable_t Material::default_parameters({
    std::make_pair(MATERIAL_PARAMETER_ROUGHNESS, 0.5f),
    std::make_pair(MATERIAL_PARAMETER_METALNESS, 0.1f),
    std::make_pair(MATERIAL_PARAMETER_SUBSURFACE, 0.0f),
    std::make_pair(MATERIAL_PARAMETER_SPECULAR, 0.25f),
    std::make_pair(MATERIAL_PARAMETER_SPECULAR_TINT, 0.1f),
    std::make_pair(MATERIAL_PARAMETER_ANISOTROPIC, 0.0f),
    std::make_pair(MATERIAL_PARAMETER_SHEEN, 0.0f),
    std::make_pair(MATERIAL_PARAMETER_SHEEN_TINT, 0.0f),
    std::make_pair(MATERIAL_PARAMETER_CLEARCOAT, 0.0f),
    std::make_pair(MATERIAL_PARAMETER_CLEARCOAT_GLOSS, 0.0f),
    std::make_pair(MATERIAL_PARAMETER_EMISSIVENESS, 0.0f),
    std::make_pair(MATERIAL_PARAMETER_PARALLAX_HEIGHT, 0.2f),
    std::make_pair(MATERIAL_PARAMETER_FLIP_UV, Vector2(0)),
    std::make_pair(MATERIAL_PARAMETER_UV_SCALE, Vector2(1)),
});

const decltype(Material::material_texture_names) Material::material_texture_names({
    std::make_pair(MATERIAL_TEXTURE_DIFFUSE_MAP, "DiffuseMap"),
    std::make_pair(MATERIAL_TEXTURE_NORMAL_MAP, "NormalMap"),
    std::make_pair(MATERIAL_TEXTURE_AO_MAP, "AoMap"),
    std::make_pair(MATERIAL_TEXTURE_PARALLAX_MAP, "ParallaxMap"),
    std::make_pair(MATERIAL_TEXTURE_METALNESS_MAP, "MetalnessMap"),
    std::make_pair(MATERIAL_TEXTURE_ROUGHNESS_MAP, "RoughnessMap"),
    std::make_pair(MATERIAL_TEXTURE_SKYBOX_MAP, "SkyboxMap"),
    std::make_pair(MATERIAL_TEXTURE_COLOR_MAP, "ColorMap"),
    std::make_pair(MATERIAL_TEXTURE_POSITION_MAP, "PositionMap"),
    std::make_pair(MATERIAL_TEXTURE_DATA_MAP, "DataMap"),
    std::make_pair(MATERIAL_TEXTURE_SSAO_MAP, "SSLightingMap"),
    std::make_pair(MATERIAL_TEXTURE_TANGENT_MAP, "TangentMap"),
    std::make_pair(MATERIAL_TEXTURE_BITANGENT_MAP, "BitangentMap"),
    std::make_pair(MATERIAL_TEXTURE_DEPTH_MAP, "DepthMap"),
    std::make_pair(MATERIAL_TEXTURE_SPLAT_MAP, "SplatMap"),
    std::make_pair(MATERIAL_TEXTURE_BASE_TERRAIN_COLOR_MAP, "BaseTerrainColorMap"),
    std::make_pair(MATERIAL_TEXTURE_BASE_TERRAIN_NORMAL_MAP, "BaseTerrainNormalMap"),
    std::make_pair(MATERIAL_TEXTURE_BASE_TERRAIN_AO_MAP, "BaseTerrainAoMap"),
    std::make_pair(MATERIAL_TEXTURE_BASE_TERRAIN_PARALLAX_MAP, "BaseTerrainParallaxMap"),
    std::make_pair(MATERIAL_TEXTURE_TERRAIN_LEVEL1_COLOR_MAP, "Terrain1ColorMap"),
    std::make_pair(MATERIAL_TEXTURE_TERRAIN_LEVEL1_NORMAL_MAP, "Terrain1NormalMap"),
    std::make_pair(MATERIAL_TEXTURE_TERRAIN_LEVEL1_AO_MAP, "Terrain2AoMap"),
    std::make_pair(MATERIAL_TEXTURE_TERRAIN_LEVEL1_PARALLAX_MAP, "Terrain1ParallaxMap"),
    std::make_pair(MATERIAL_TEXTURE_TERRAIN_LEVEL2_COLOR_MAP, "Terrain2ColorMap"),
    std::make_pair(MATERIAL_TEXTURE_TERRAIN_LEVEL2_NORMAL_MAP, "Terrain2NormalMap"),
    std::make_pair(MATERIAL_TEXTURE_TERRAIN_LEVEL2_AO_MAP, "Terrain2AoMap"),
    std::make_pair(MATERIAL_TEXTURE_TERRAIN_LEVEL2_PARALLAX_MAP, "Terrain2ParallaxMap")
});

MaterialParameter::MaterialParameter()
    : type(MaterialParameter_None),
      values({ 0.0f })
{
}

MaterialParameter::MaterialParameter(float value)
    : type(MaterialParameter_Float),
      values({ value, 0.0f, 0.0f, 0.0f })
{
}

MaterialParameter::MaterialParameter(float x, float y)
    : type(MaterialParameter_Vector2),
      values({ x, y, 0.0f, 0.0f })
{
}

MaterialParameter::MaterialParameter(float x, float y, float z)
    : type(MaterialParameter_Vector3),
      values({ x, y, z, 0.0f })
{
}

MaterialParameter::MaterialParameter(float x, float y, float z, float w)
    : type(MaterialParameter_Vector4),
      values({ x, y, z, w })
{
}
MaterialParameter::MaterialParameter(const Vector2 &value) : MaterialParameter(value.x, value.y) {}
MaterialParameter::MaterialParameter(const Vector3 &value) : MaterialParameter(value.x, value.y, value.z) {}
MaterialParameter::MaterialParameter(const Vector4 &value) : MaterialParameter(value.x, value.y, value.z, value.w) {}

MaterialParameter::MaterialParameter(const float *data, size_t nvalues, MaterialParameterType paramtype)
    : type(paramtype)
{
    for (size_t i = 0; i < values.size(); i++) {
        if (i < nvalues) {
            values[i] = data[i];
        } else {
            values[i] = 0.0f;
        }
    }
}

MaterialParameter::MaterialParameter(const MaterialParameter &other)
    : type(other.type),
      values(other.values)
{
}

Material::Material()
    : fbom::FBOMLoadable(fbom::FBOMObjectType("MATERIAL")),
      diffuse_color(Vector4(1.0))
{
    for (size_t i = 0; i < default_parameters.Size(); i++) {
        auto pair = default_parameters.KeyValueAt(i);
        m_params.Set(pair.first, pair.second);
    }
}

Material::Material(const Material &other)
    : fbom::FBOMLoadable(fbom::FBOMObjectType("MATERIAL")),
      m_params(other.m_params),
      m_textures(other.m_textures),
      alpha_blended(other.alpha_blended),
      cull_faces(other.cull_faces),
      depth_test(other.depth_test),
      depth_write(other.depth_write),
      diffuse_color(other.diffuse_color)
{
}

void Material::SetParameter(MaterialParameterKey key, float value)
{
    float values[] = { value };
    m_params.Set(key, MaterialParameter(values, 1, MaterialParameter_Float));
}

void Material::SetParameter(MaterialParameterKey key, int value)
{
    float values[] = { (float)value };
    m_params.Set(key, MaterialParameter(values, 1, MaterialParameter_Int));
}

void Material::SetParameter(MaterialParameterKey key, const Vector2 &value)
{
    float values[] = { value.x, value.y };
    m_params.Set(key, MaterialParameter(values, 2, MaterialParameter_Vector2));
}

void Material::SetParameter(MaterialParameterKey key, const Vector3 &value)
{
    float values[] = { value.x, value.y, value.z };
    m_params.Set(key, MaterialParameter(values, 3, MaterialParameter_Vector3));
}

void Material::SetParameter(MaterialParameterKey key, const Vector4 &value)
{
    float values[] = { value.x, value.y, value.z, value.w };
    m_params.Set(key, MaterialParameter(values, 4, MaterialParameter_Vector4));
}

void Material::SetTexture(MaterialTextureKey key, const std::shared_ptr<Texture> &texture)
{
    m_textures.Set(key, MaterialTextureWrapper(texture));
}

std::shared_ptr<Texture> Material::GetTexture(MaterialTextureKey key) const
{
    return m_textures.Get(key).m_texture;
}

std::shared_ptr<Loadable> Material::Clone()
{
    return CloneImpl();
}

std::shared_ptr<Material> Material::CloneImpl()
{
    return std::make_shared<Material>(*this);
}

} // namespace hyperion
