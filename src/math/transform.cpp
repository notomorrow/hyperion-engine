#include "transform.h"

namespace hyperion {

const Transform Transform::identity{};

Transform::Transform()
    : m_translation(Vector3::Zero()),
      m_scale(Vector3::One()),
      m_rotation(Quaternion::Identity())
{
    UpdateMatrix();
}

Transform::Transform(const Vector3 &translation, const Vector3 &scale, const Quaternion &rotation)
    : m_translation(translation),
      m_scale(scale),
      m_rotation(rotation)
{
    UpdateMatrix();
}

Transform::Transform(const Transform &other)
    : m_translation(other.m_translation),
      m_scale(other.m_scale),
      m_rotation(other.m_rotation),
      m_matrix(other.m_matrix)
{
    UpdateMatrix();
}

void Transform::UpdateMatrix()
{
    Matrix4 S, R, T;

    MatrixUtil::ToTranslation(T, m_translation);
    MatrixUtil::ToRotation(R, m_rotation);
    MatrixUtil::ToScaling(S, m_scale);

    m_matrix = S * R * T;
}

Transform Transform::operator*(const Transform &other) const
{
    return Transform(
        m_translation + other.m_translation,
        m_scale * other.m_scale,
        m_rotation * other.m_rotation
    );
}

Transform &Transform::operator*=(const Transform &other)
{
    m_translation += other.m_translation;
    m_scale *= other.m_scale;
    m_rotation *= other.m_rotation;

    UpdateMatrix();

    return *this;
}

} // namespace hyperion
