#include "camera.h"

namespace hyperion {

Camera::Camera(int width, int height, float _near, float _far)
    : m_width(width),
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
}

void Camera::Rotate(const Vector3 &axis, float radians)
{
    m_direction.Rotate(axis, radians);
    m_direction.Normalize();
}

void Camera::UpdateFrustum()
{
    m_frustum.SetViewProjectionMatrix(m_view_proj_mat);
}

void Camera::Update(double dt)
{
    UpdateLogic(dt);
    UpdateMatrices();

    m_view_proj_mat = m_view_mat * m_proj_mat;

    UpdateFrustum();
}

} // namespace hyperion
