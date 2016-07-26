#include "quaternion.h"

namespace apex {
Quaternion::Quaternion()
    : x(0.0), y(0.0), z(0.0), w(1.0)
{
}

Quaternion::Quaternion(float x, float y, float z, float w)
    : x(x), y(y), z(z), w(w)
{
}

Quaternion::Quaternion(const Matrix4 &mat)
{
    const float xx = mat(0, 0),
        xy = mat(1, 0),
        xz = mat(2, 0),

        yx = mat(0, 1),
        yy = mat(1, 1),
        yz = mat(2, 1),

        zx = mat(0, 2),
        zy = mat(1, 2),
        zz = mat(2, 2);

    float amt = xx + yy + zz;
    if (amt >= 0.0f) {
        float s = sqrt(amt + 1);
        w = 0.5f * s;
        x = (zy - yz) * (0.5f / s);
        y = (xz - zx) * (0.5f / s);
        z = (yx - xy) * (0.5f / s);
    } else if ((xx > yy) && (xx > zz)) {
        float s = sqrt(1 + xx - yy - zz);
        x = s * 0.5f;
        y = (yx + xy) * (0.5f / s);
        z = (xz + zx) * (0.5f / s);
        w = (zy - yz) * (0.5f / s);
    } else if (yy > zz) {
        float s = sqrt(1 + yy - xx - zz);
        y = s * 0.5f;
        x = (yx + xy) * (0.5f / s);
        z = (zy + yz) * (0.5f / s);
        w = (xz - zx) * (0.5f / s);
    } else {
        float s = sqrt(1 + zz - xx - yy);
        z = s * 0.5f;
        x = (xz + zx) * (0.5f / s);
        y = (zy + yz) * (0.5f / s);
        w = (yx - xy) * (0.5f / s);
    }
}

Quaternion::Quaternion(const Vector3 &axis, float radians)
{
    Vector3 tmp(axis);

    if (tmp.Length() != 1) {
        tmp.Normalize();
    }

    if (tmp != Vector3::Zero()) {
        float half_angle = radians / 2;
        float sin_half_angle = sin(half_angle);

        x = sin_half_angle * tmp.x;
        y = sin_half_angle * tmp.y;
        z = sin_half_angle * tmp.z;
        w = cos(half_angle);
    } else {
        (*this) = Quaternion::Identity();
    }
}

Quaternion::Quaternion(const Quaternion &other)
    : x(other.x), y(other.y), z(other.z), w(other.w)
{
}

Quaternion &Quaternion::operator=(const Quaternion &other)
{
    x = other.x;
    y = other.y;
    z = other.z;
    w = other.w;
    return *this;
}

Quaternion Quaternion::operator*(const Quaternion &other) const
{
    float x1 = x * other.w + y * other.z - z * other.y + w * other.x;
    float y1 = -x * other.z + y * other.w + z * other.x + w * other.y;
    float z1 = x * other.y - y * other.x + z * other.w + w * other.z;
    float w1 = -x * other.x - y * other.y - z * other.z + w * other.w;
    return Quaternion(x1, y1, z1, w1);
}

Quaternion &Quaternion::operator*=(const Quaternion &other)
{
    float x1 = x * other.w + y * other.z - z * other.y + w * other.x;
    float y1 = -x * other.z + y * other.w + z * other.x + w * other.y;
    float z1 = x * other.y - y * other.x + z * other.w + w * other.z;
    float w1 = -x * other.x - y * other.y - z * other.z + w * other.w;
    x = x1;
    y = y1;
    z = z1;
    w = w1;
    return *this;
}

Vector3 Quaternion::operator*(const Vector3 &vec) const
{
    Vector3 result;
    result.x = w * w * vec.x + 2 * y * w * vec.z - 2 * z * w * vec.y + x * x * vec.x
        + 2 * y * x * vec.y + 2 * z * x * vec.z - z * z * vec.x - y * y * vec.x;
    result.y = 2 * x * y * vec.x + y * y * vec.y + 2 * z * y * vec.z + 2 * w * z
        * vec.x - z * z * vec.y + w * w * vec.y - 2 * x * w * vec.z - x * x
        * vec.y;

    result.z = 2 * x * z * vec.x + 2 * y * z * vec.y + z * z * vec.z - 2 * w * y * vec.x
        - y * y * vec.z + 2 * w * x * vec.y - x * x * vec.z + w * w * vec.z;
    return result;
}

float Quaternion::Length() const
{
    return w * w + x * x + y * y + z * z;
}

Quaternion &Quaternion::Invert()
{
    float len = Length();
    if (len > 0.0) {
        float inv_len = 1.0f / len;
        w = w * inv_len;
        x = -x * inv_len;
        y = -y * inv_len;
        z = -z * inv_len;
    }
    return *this;
}

Quaternion &Quaternion::Slerp(const Quaternion &to, float amt)
{
    float cos_half_theta = w * to.w + x * to.x + y * to.y + z * to.z;

    if (abs(cos_half_theta) >= 1.0f) {
        return *this;
    }

    float half_theta = acos(cos_half_theta);
    float sin_half_theta = sqrt(1.0f - cos_half_theta * cos_half_theta);

    if (abs(sin_half_theta) < 0.001f) {
        w = w * 0.5f + to.w * 0.5f;
        x = x * 0.5f + to.x * 0.5f;
        y = y * 0.5f + to.y * 0.5f;
        z = z * 0.5f + to.z * 0.5f;
        return *this;
    }

    float ratio_a = sin((1.0f - amt) * half_theta) / sin_half_theta;
    float ratio_b = sin(amt * half_theta) / sin_half_theta;

    w = w * ratio_a + to.w * ratio_b;
    x = x * ratio_a + to.x * ratio_b;
    y = y * ratio_a + to.y * ratio_b;
    z = z * ratio_a + to.z * ratio_b;
    return *this;
}

int Quaternion::GimbalPole() const
{
    float amt = y * x + z * w;
    return amt > 0.499f ? 1 : (amt < -0.499f ? -1 : 0);
}

float Quaternion::Roll() const
{
    int pole = GimbalPole();
    return pole == 0 ? atan2(2.0f * (w * z + y * x), 1.0f - 2.0f * (x * x + z * z)) : pole * 2.0f * atan2(y, w);
}

float Quaternion::Pitch() const
{
    int pole = GimbalPole();
    return pole == 0 ? 
        asin(MathUtil::Clamp(2.0f * (w * x - z * y), -1.0f, 1.0f)) : 
        pole * MathUtil::PI * 0.5f;
}

float Quaternion::Yaw() const
{
    int pole = GimbalPole();
    return pole == 0 ? atan2(2.0f * (y * w + x * z), 1.0f - 2.0f * (y * y + x * x)) : 0.0f;
}

Quaternion Quaternion::Identity()
{
    return Quaternion(0.0, 0.0, 0.0, 1.0);
}
}