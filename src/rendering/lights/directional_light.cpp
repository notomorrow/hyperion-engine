#include "directional_light.h"
#include "../shader.h"

namespace hyperion {
DirectionalLight::DirectionalLight()
    : direction(0.57735f), color(1.0f), m_intensity(1.0f)
{
}

DirectionalLight::DirectionalLight(const Vector3 &direction, const Vector4 &color, float intensity)
    : direction(direction), color(color), m_intensity(intensity)
{
}

const Vector3 &DirectionalLight::GetDirection() const
{
    return direction;
}

void DirectionalLight::SetDirection(const Vector3 &dir)
{
    direction = dir;
}

const Vector4 &DirectionalLight::GetColor() const
{
    return color;
}

void DirectionalLight::SetColor(const Vector4 &col)
{
    color = col;
}

void DirectionalLight::Bind(int index, Shader *shader) const
{
    shader->SetUniform("env_DirectionalLight.direction", direction);
    shader->SetUniform("env_DirectionalLight.color", color);
    shader->SetUniform("env_DirectionalLight.intensity", m_intensity);
}
}
