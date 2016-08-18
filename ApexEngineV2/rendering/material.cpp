#include "material.h"

namespace apex {
MaterialParameter::MaterialParameter()
    : size(0), type(MaterialParameterType::MATPARAM_NONE)
{
}

MaterialParameter::MaterialParameter(const float *data, size_t nvalues, MaterialParameterType paramtype)
{
    for (size_t i = 0; i < nvalues; i++) {
        values[i] = data[i];
    }
    size = nvalues;
    type = paramtype;
}

MaterialParameter::MaterialParameter(const MaterialParameter &other)
{
    values = other.values;
    size = other.size;
    type = other.type;
}

bool MaterialParameter::IsEmpty() const
{
    return size == 0;
}

size_t MaterialParameter::NumValues() const
{
    return size;
}

MaterialParameter::MaterialParameterType MaterialParameter::GetType() const
{
    return type;
}

float &MaterialParameter::operator[](size_t idx)
{
    return values[idx];
}

float MaterialParameter::operator[](size_t idx) const
{
    return values[idx];
}

Material::Material()
{
}

Material::Material(const Material &other)
    : params(other.params),
      alpha_blended(other.alpha_blended),
      depth_test(other.depth_test),
      depth_write(other.depth_write),
      diffuse_texture(other.diffuse_texture),
      normals_texture(other.normals_texture)
{
}

bool Material::HasParameter(const std::string &name) const
{
    auto it = params.find(name);
    if (it != params.end() &&
        it->second.GetType() != MaterialParameter::MATPARAM_NONE) {
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
    params[name] = MaterialParameter(values, 1, MaterialParameter::MATPARAM_FLOAT);
}

void Material::SetParameter(const std::string &name, int value)
{
    float values[] = { (float)value };
    params[name] = MaterialParameter(values, 1, MaterialParameter::MATPARAM_INT);
}

void Material::SetParameter(const std::string &name, const std::shared_ptr<Texture> &value)
{
    float values[] = { (float)value->GetId() };
    params[name] = MaterialParameter(values, 1, MaterialParameter::MATPARAM_TEXHANDLE);
}

void Material::SetParameter(const std::string &name, const Vector2 &value)
{
    float values[] = { value.x, value.y };
    params[name] = MaterialParameter(values, 2, MaterialParameter::MATPARAM_VEC2);
}

void Material::SetParameter(const std::string &name, const Vector3 &value)
{
    float values[] = { value.x, value.y, value.z };
    params[name] = MaterialParameter(values, 3, MaterialParameter::MATPARAM_VEC3);
}

void Material::SetParameter(const std::string &name, const Vector4 &value)
{
    float values[] = { value.x, value.y, value.z, value.w };
    params[name] = MaterialParameter(values, 4, MaterialParameter::MATPARAM_VEC4);
}
}