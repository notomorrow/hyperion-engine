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

Transform::Transform(const Vector3 &translation)
    : Transform(translation, Vector3::One(), Quaternion::Identity())
{
}

Transform::Transform(const Transform &other)
    : m_translation(other.m_translation),
      m_scale(other.m_scale),
      m_rotation(other.m_rotation),
      m_matrix(other.m_matrix)
{
}

void Transform::UpdateMatrix()
{
    Matrix4 s, r, t;

    MatrixUtil::ToTranslation(t, m_translation);
    MatrixUtil::ToRotation(r, m_rotation);
    MatrixUtil::ToScaling(s, m_scale);

    m_matrix = s * r * t;
}

Transform Transform::operator*(const Transform &other) const
{
    return {
        m_translation + other.m_translation,
        m_scale * other.m_scale,
        m_rotation * other.m_rotation
    };
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
