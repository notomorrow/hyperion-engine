#include "directional_light.h"
#include "../shader.h"

namespace apex {
DirectionalLight::DirectionalLight()
    : direction(0.57735), color(1.0)
{
}

DirectionalLight::DirectionalLight(const Vector3 &direction, const Vector4 &color)
    : direction(direction), color(color)
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

void DirectionalLight::BindLight(int index, Shader *shader)
{
    shader->SetUniform("Env_DirectionalLight.direction", direction);
    shader->SetUniform("Env_DirectionalLight.color", color);
}
}