#include "camera.h"

namespace apex {
Camera::Camera(int width, int height)
    : width(width), height(height)
{
    translation = Vector3::Zero();
    direction = Vector3::UnitZ();
    up = Vector3::UnitY();
}

int Camera::GetWidth() const
{
    return width;
}

void Camera::SetWidth(int w)
{
    width = w;
}

int Camera::GetHeight() const
{
    return height;
}

void Camera::SetHeight(int h)
{
    height = h;
}

const Vector3 &Camera::GetTranslation() const
{
    return translation;
}

void Camera::SetTranslation(const Vector3 &vec)
{
    translation = vec;
}

const Vector3 &Camera::GetDirection() const
{
    return direction;
}

void Camera::SetDirection(const Vector3 &vec)
{
    direction = vec;
}

const Vector3 &Camera::GetUpVector() const
{
    return up;
}

void Camera::SetUpVector(const Vector3 &vec)
{
    up = vec;
}

Matrix4 &Camera::GetViewMatrix()
{
    return view_mat;
}

Matrix4 &Camera::GetProjectionMatrix()
{
    return proj_mat;
}

Matrix4 &Camera::GetViewProjectionMatrix()
{
    return view_proj_mat;
}

void Camera::Rotate(const Vector3 &axis, float radians)
{
    direction.Rotate(axis, radians);
    direction.Normalize();
}

void Camera::Update(double dt)
{
    UpdateLogic(dt);
    UpdateMatrices();
}
}