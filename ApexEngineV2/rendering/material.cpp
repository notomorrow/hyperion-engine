#include "material.h"

namespace apex {

const std::map<std::string, MaterialParameter> Material::default_parameters = {
    { "roughness", MaterialParameter(0.6f) },
    { "shininess", MaterialParameter(0.1f) }
};

MaterialParameter::MaterialParameter()
    : size(0),
      type(MaterialParameter_None)
{
}

MaterialParameter::MaterialParameter(const float value)
    : size(1),
      type(MaterialParameter_Float)
{
    values[0] = value;
}

MaterialParameter::MaterialParameter(const float *data, size_t nvalues, MaterialParameterType paramtype)
    : size(nvalues),
      type(paramtype)
{
    for (size_t i = 0; i < nvalues; i++) {
        values[i] = data[i];
    }
}

MaterialParameter::MaterialParameter(const MaterialParameter &other)
    : values(other.values),
      size(other.size),
      type(other.type)
{
}

Material::Material()
{
    for (auto it : default_parameters) {
        params[it.first] = it.second;
    }
}

Material::Material(const Material &other)
    : params(other.params),
      alpha_blended(other.alpha_blended),
      depth_test(other.depth_test),
      depth_write(other.depth_write),
      diffuse_color(other.diffuse_color),
      texture0(other.texture0),
      texture1(other.texture1),
      texture2(other.texture2),
      texture3(other.texture3),
      normals0(other.normals0),
      normals1(other.normals1),
      normals2(other.normals2),
      normals3(other.normals3)
{
}

bool Material::HasParameter(const std::string &name) const
{
    auto it = params.find(name);
    if (it != params.end() && it->second.GetType() != MaterialParameter_None) {
        return true;
    }
    return false;
}

std::map<std::string, MaterialParameter> &Material::GetParameters()
{
    return params;
}

const MaterialParameter &Material::GetParameter(const std::string &name) const
{
    auto it = params.find(name);
    if (it == params.end()) {
        throw std::out_of_range("parameter not found");
    }
    return it->second;
}

void Material::SetParameter(const std::string &name, float value)
{
    float values[] = { value };
    params[name] = MaterialParameter(values, 1, MaterialParameter_Float);
}

void Material::SetParameter(const std::string &name, int value)
{
    float values[] = { (float)value };
    params[name] = MaterialParameter(values, 1, MaterialParameter_Int);
}

void Material::SetParameter(const std::string &name, const std::shared_ptr<Texture> &value)
{
    float values[] = { (float)value->GetId() };
    params[name] = MaterialParameter(values, 1, MaterialParameter_Texture);
}

void Material::SetParameter(const std::string &name, const Vector2 &value)
{
    float values[] = { value.x, value.y };
    params[name] = MaterialParameter(values, 2, MaterialParameter_Vector2);
}

void Material::SetParameter(const std::string &name, const Vector3 &value)
{
    float values[] = { value.x, value.y, value.z };
    params[name] = MaterialParameter(values, 3, MaterialParameter_Vector3);
}

void Material::SetParameter(const std::string &name, const Vector4 &value)
{
    float values[] = { value.x, value.y, value.z, value.w };
    params[name] = MaterialParameter(values, 4, MaterialParameter_Vector4);
}

} // namespace apex
