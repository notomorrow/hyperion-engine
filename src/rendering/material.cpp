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
    std::make_pair(MATERIAL_PARAMETER_PARALLAX_HEIGHT, 0.15f),
    std::make_pair(MATERIAL_PARAMETER_FLIP_UV, Vector2(0)),
    std::make_pair(MATERIAL_PARAMETER_UV_SCALE, Vector2(1)),
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

        if (default_parameters.Has(pair.first)) {
            m_params.Set(pair.first, pair.second);
        }
    }
}

Material::Material(const Material &other)
    : fbom::FBOMLoadable(fbom::FBOMObjectType("MATERIAL")),
      m_params(other.m_params),
      alpha_blended(other.alpha_blended),
      cull_faces(other.cull_faces),
      depth_test(other.depth_test),
      depth_write(other.depth_write),
      diffuse_color(other.diffuse_color),
      textures(other.textures)
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

void Material::SetTexture(const std::string &name, const std::shared_ptr<Texture> &texture)
{
    textures[name] = texture;
}

std::shared_ptr<Texture> Material::GetTexture(const std::string &name) const
{
    const auto it = textures.find(name);

    if (it != textures.end()) {
        return it->second;
    }

    return nullptr;
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
