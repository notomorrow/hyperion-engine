/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Vector3.hpp>
#include <core/math/Quaternion.hpp>
#include <core/math/Matrix3.hpp>
#include <core/math/Matrix4.hpp>

#include <core/object/HypClassUtils.hpp>

#include <cmath>

namespace hyperion {

HYP_BEGIN_STRUCT(Vec3f, -1, 0, {})

HypField(NAME(HYP_STR(x)), &Type::x, offsetof(Type, x)),
    HypField(NAME(HYP_STR(y)), &Type::y, offsetof(Type, y)),
    HypField(NAME(HYP_STR(z)), &Type::z, offsetof(Type, z))
        HYP_END_STRUCT

HYP_BEGIN_STRUCT(Vec3i, -1, 0, {})
HypField(NAME(HYP_STR(x)), &Type::x, offsetof(Type, x)),
    HypField(NAME(HYP_STR(y)), &Type::y, offsetof(Type, y)),
    HypField(NAME(HYP_STR(z)), &Type::z, offsetof(Type, z))
        HYP_END_STRUCT

HYP_BEGIN_STRUCT(Vec3u, -1, 0, {})
HypField(NAME(HYP_STR(x)), &Type::x, offsetof(Type, x)),
    HypField(NAME(HYP_STR(y)), &Type::y, offsetof(Type, y)),
    HypField(NAME(HYP_STR(z)), &Type::z, offsetof(Type, z))
        HYP_END_STRUCT

Vec3<float> math::Vec3<float>::operator*(const Matrix3& mat) const
{
    return {
        x * mat.values[0] + y * mat.values[3] + z * mat.values[6],
        x * mat.values[1] + y * mat.values[4] + z * mat.values[7],
        x * mat.values[2] + y * mat.values[5] + z * mat.values[8]
    };
}

Vec3<float>& math::Vec3<float>::operator*=(const Matrix3& mat)
{
    return operator=(operator*(mat));
}

Vec3<float> math::Vec3<float>::operator*(const Matrix4& mat) const
{
    Vector4 product {
        x * mat.values[0] + y * mat.values[4] + z * mat.values[8] + mat.values[12],
        x * mat.values[1] + y * mat.values[5] + z * mat.values[9] + mat.values[13],
        x * mat.values[2] + y * mat.values[6] + z * mat.values[10] + mat.values[14],
        x * mat.values[3] + y * mat.values[7] + z * mat.values[11] + mat.values[15]
    };

    product /= product.w;

    return {
        product.x,
        product.y,
        product.z
    };
}

Vec3<float>& math::Vec3<float>::operator*=(const Matrix4& mat)
{
    return operator=(operator*(mat));
}

Vec3<float> math::Vec3<float>::operator*(const Quaternion& quat) const
{
    Vec3<float> result;
    result.x = quat.w * quat.w * x + 2 * quat.y * quat.w * z - 2 * quat.z * quat.w * y + quat.x * quat.x * x + 2 * quat.y * quat.x * y + 2 * quat.z * quat.x * z - quat.z * quat.z * x - quat.y * quat.y * x;

    result.y = 2 * quat.x * quat.y * x + quat.y * quat.y * y + 2 * quat.z * quat.y * z + 2 * quat.w * quat.z * x - quat.z * quat.z * y + quat.w * quat.w * y - 2 * quat.x * quat.w * z - quat.x * quat.x * y;

    result.z = 2 * quat.x * quat.z * x + 2 * quat.y * quat.z * y + quat.z * quat.z * z - 2 * quat.w * quat.y * x - quat.y * quat.y * z + 2 * quat.w * quat.x * y - quat.x * quat.x * z + quat.w * quat.w * z;
    return result;
}

Vec3<float>& math::Vec3<float>::operator*=(const Quaternion& quat)
{
    return operator=(operator*(quat));
}

float math::Vec3<float>::DistanceSquared(const Vec3f& other) const
{
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    return dx * dx + dy * dy + dz * dz;
}

/* Euclidean distance */
float math::Vec3<float>::Distance(const Vec3f& other) const
{
    return MathUtil::Sqrt(DistanceSquared(other));
}

Vec3<float> math::Vec3<float>::Normalized() const
{
    return *this / MathUtil::Max(Length(), MathUtil::epsilon_f);
}

Vec3<float>& math::Vec3<float>::Normalize()
{
    return *this /= MathUtil::Max(Length(), MathUtil::epsilon_f);
}

Vec3<float> math::Vec3<float>::Cross(const Vec3<float>& other) const
{
    return {
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    };
}

Vec3<float> math::Vec3<float>::Reflect(const Vec3<float>& normal) const
{
    const Vec3& incident = *this;
    return incident - Vec3(2.0f) * Dot(normal) * normal;
}

Vec3<float>& math::Vec3<float>::Rotate(const Vec3<float>& axis, float radians)
{
    return (*this) = Matrix4::Rotation(axis, radians) * (*this);
}

Vec3<float>& math::Vec3<float>::Rotate(const Quaternion& quaternion)
{
    return (*this) = Matrix4::Rotation(quaternion) * (*this);
}

Vec3<float>& math::Vec3<float>::Lerp(const Vec3<float>& to, const float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    z = MathUtil::Lerp(z, to.z, amt);

    return *this;
}

float math::Vec3<float>::Dot(const Vec3<float>& other) const
{
    return x * other.x + y * other.y + z * other.z;
}

float math::Vec3<float>::AngleBetween(const Vector3& other) const
{
    const float dot_product = x * other.x + y * other.y + z * other.z;
    const float arc_cos = MathUtil::Arccos(dot_product);

    return arc_cos / (Length() * other.Length());
}

Vec3<float> math::Vec3<float>::Abs(const Vec3<float>& vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z)
    };
}

Vec3<float> math::Vec3<float>::Round(const Vec3<float>& vec)
{
    return {
        MathUtil::Round(vec.x),
        MathUtil::Round(vec.y),
        MathUtil::Round(vec.z)
    };
}

Vec3<float> math::Vec3<float>::Clamp(const Vec3<float>& vec, float min_value, float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vec3<float> math::Vec3<float>::Min(const Vec3<float>& a, const Vec3<float>& b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z)
    };
}

Vec3<float> math::Vec3<float>::Max(const Vec3<float>& a, const Vec3<float>& b)
{
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z)
    };
}

} // namespace hyperion
