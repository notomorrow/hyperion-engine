#include "transform.h"

namespace apex {
Transform::Transform()
    : translation(Vector3::Zero()),
      scale(Vector3::One()),
      rotation(Quaternion::Identity())
{
    UpdateMatrix();
}

Transform::Transform(const Vector3 &translation, const Vector3 &scale, const Quaternion &rotation)
    : translation(translation),
      scale(scale),
      rotation(rotation)
{
    UpdateMatrix();
}

Transform::Transform(const Transform &other)
    : translation(other.translation),
      scale(other.scale),
      rotation(other.rotation),
      matrix(other.matrix)
{
    UpdateMatrix();
}

void Transform::UpdateMatrix()
{
    Matrix4 S, R, T;

    MatrixUtil::ToTranslation(T, translation);
    MatrixUtil::ToRotation(R, rotation);
    MatrixUtil::ToScaling(S, scale);

    matrix = S * R * T;
}
} // namespace apex