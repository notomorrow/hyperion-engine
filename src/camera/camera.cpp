#include "camera.h"

namespace hyperion {

Camera::Camera(CameraType camera_type, int width, int height, float _near, float _far)
    : m_camera_type(camera_type),
      m_width(width),
      m_height(height),
      m_near(_near),
      m_far(_far),
      m_translation(Vector3::Zero()),
      m_direction(Vector3::UnitZ()),
      m_up(Vector3::UnitY())
{
}

void Camera::SetTranslation(const Vector3 &translation)
{
    m_translation = translation;

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetDirection(const Vector3 &direction)
{
    m_direction = direction;

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetUpVector(const Vector3 &up)
{
    m_up = up;

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::Rotate(const Vector3 &axis, float radians)
{
    m_direction.Rotate(axis, radians);
    m_direction.Normalize();

    UpdateViewMatrix();
    UpdateViewProjectionMatrix();
}

void Camera::SetViewMatrix(const Matrix4 &view_mat)
{
    m_view_mat = view_mat;

    UpdateViewProjectionMatrix();
}

void Camera::SetProjectionMatrix(const Matrix4 &proj_mat)
{
    m_proj_mat = proj_mat;

    UpdateViewProjectionMatrix();
}

void Camera::SetViewProjectionMatrix(const Matrix4 &view_mat, const Matrix4 &proj_mat)
{
    m_view_mat = view_mat;
    m_proj_mat = proj_mat;

    UpdateViewProjectionMatrix();
}

void Camera::UpdateViewProjectionMatrix()
{
    m_view_proj_mat = m_view_mat * m_proj_mat;

    m_frustum.SetFromViewProjectionMatrix(m_view_proj_mat);
}

void Camera::Update(double dt)
{
    UpdateLogic(dt);
    UpdateMatrices();
}

void Camera::UpdateMatrices()
{
    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateViewProjectionMatrix();
}

} // namespace hyperion
