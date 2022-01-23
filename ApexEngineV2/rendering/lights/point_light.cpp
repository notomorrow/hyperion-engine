#include "./point_light.h"
#include "../shader.h"

namespace hyperion {

PointLight::PointLight()
    : m_position(0.0f),
      m_color(1.0f),
      m_radius(5.0f)
{
}

PointLight::PointLight(const PointLight &other)
    : m_position(other.m_position),
      m_color(other.m_color),
      m_radius(other.m_radius)
{
}

PointLight::PointLight(const Vector3 &position, const Vector4 &color, float radius)
    : m_position(position),
      m_color(color),
      m_radius(radius)
{
}

void PointLight::Bind(int index, Shader *shader)
{
    const std::string index_str = std::to_string(index);

    shader->SetUniform("env_PointLights[" + index_str + "].position", m_position);
    shader->SetUniform("env_PointLights[" + index_str + "].color", m_color);
    shader->SetUniform("env_PointLights[" + index_str + "].radius", m_radius);
}

} // namespace hyperion
