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

const Vector3 &Transform::GetTranslation() const
{
    return translation;
}

void Transform::SetTranslation(const Vector3 &vec)
{
    translation = vec;
    UpdateMatrix();
}

const Vector3 &Transform::GetScale() const
{
    return scale;
}

void Transform::SetScale(const Vector3 &vec)
{
    scale = vec;
    UpdateMatrix();
}

const Quaternion &Transform::GetRotation() const
{
    return rotation;
}

void Transform::SetRotation(const Quaternion &rot)
{
    rotation = rot;
    UpdateMatrix();
}

const Matrix4 &Transform::GetMatrix() const
{
    return matrix;
}

void Transform::UpdateMatrix()
{
    Matrix4 T, R, S;

    MatrixUtil::ToTranslation(T, translation);
    MatrixUtil::ToRotation(R, rotation);
    MatrixUtil::ToScaling(S, scale);

    matrix = S * R * T;
}
}