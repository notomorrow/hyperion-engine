#include "matrix4.h"
#include "vector3.h"
#include "quaternion.h"

#include <string.h>

namespace hyperion {

Matrix4 Matrix4::Translation(const Vector3 &translation)
{
    auto mat = Identity();

    mat(0, 3) = translation.x;
    mat(1, 3) = translation.y;
    mat(2, 3) = translation.z;

    return mat;
}

Matrix4 Matrix4::Rotation(const Quaternion &rotation)
{
    auto mat = Identity();

    float xx = rotation.x * rotation.x;
    float xy = rotation.x * rotation.y;
    float xz = rotation.x * rotation.z;
    float xw = rotation.x * rotation.w;
    float yy = rotation.y * rotation.y;
    float yz = rotation.y * rotation.z;
    float yw = rotation.y * rotation.w;
    float zz = rotation.z * rotation.z;
    float zw = rotation.z * rotation.w;

    mat(0, 0) = 1.0f - 2.0f * (yy + zz);
    mat(0, 1) = 2.0f * (xy + zw);
    mat(0, 2) = 2.0f * (xz - yw);
    mat(0, 3) = 0.0;

    mat(1, 0) = 2.0f * (xy - zw);
    mat(1, 1) = 1.0f - 2.0f * (xx + zz);
    mat(1, 2) = 2.0f * (yz + xw);
    mat(1, 3) = 0.0;

    mat(2, 0) = 2.0f * (xz + yw);
    mat(2, 1) = 2.0f * (yz - xw);
    mat(2, 2) = 1.0f - 2.0f * (xx + yy);
    mat(2, 3) = 0.0;

    mat(3, 0) = 0.0f;
    mat(3, 1) = 0.0;
    mat(3, 2) = 0.0;
    mat(3, 3) = 1.0;

    return mat;
}

Matrix4 Matrix4::Rotation(const Vector3 &axis, float radians)
{
    return Rotation(Quaternion(axis, radians));
}

Matrix4 Matrix4::Scaling(const Vector3 &scale)
{
    auto mat = Identity();

    mat(0, 0) = scale.x;
    mat(1, 1) = scale.y;
    mat(2, 2) = scale.z;

    return mat;
}

Matrix4 Matrix4::Perspective(float fov, int w, int h, float n, float f)
{
    auto mat = Identity();

    float ar = (float)w / (float)h;
    float tan_half_fov = tan(MathUtil::DegToRad(fov / 2.0f));
    float range = n - f;

    mat(0, 0) = 1.0f / (tan_half_fov * ar);
    mat(0, 1) = 0.0f;
    mat(0, 2) = 0.0f;
    mat(0, 3) = 0.0f;

    mat(1, 0) = 0.0f;
    mat(1, 1) = (1.0f / (tan_half_fov));
    mat(1, 2) = 0.0f;
    mat(1, 3) = 0.0f;

    mat(2, 0) = 0.0f;
    mat(2, 1) = 0.0f;
    mat(2, 2) = (-n - f) / range;
    mat(2, 3) = (2.0f * f * n) / range;

    mat(3, 0) = 0.0f;
    mat(3, 1) = 0.0f;
    mat(3, 2) = 1.0f;
    mat(3, 3) = 0.0f;

    return mat;
}

Matrix4 Matrix4::Orthographic(float l, float r, float b, float t, float n, float f)
{
    auto mat = Identity();

    float x_orth = 2.0f / (r - l);
    float y_orth = 2.0f / (t - b);
    float z_orth = 2.0f / (f - n);
    float tx = -1.0f * ((r + l) / (r - l));
    float ty = -1.0f * ((t + b) / (t - b));
    float tz = -1.0f * ((f + n) / (f - n));

    mat(0, 0) = x_orth;
    mat(0, 1) = 0;
    mat(0, 2) = 0;
    mat(0, 3) = tx;

    mat(1, 0) = 0;
    mat(1, 1) = y_orth;
    mat(1, 2) = 0;
    mat(1, 3) = ty;

    mat(2, 0) = 0;
    mat(2, 1) = 0;
    mat(2, 2) = z_orth;
    mat(2, 3) = tz;

    mat(3, 0) = 0;
    mat(3, 1) = 0;
    mat(3, 2) = 0;
    mat(3, 3) = 1;

    return mat;
}

Matrix4 Matrix4::LookAt(const Vector3 &dir, const Vector3 &up)
{
    auto mat = Identity();

    Vector3 l_vez = dir;
    l_vez.Normalize();

    Vector3 l_vex = dir;
    l_vex.Normalize();
    l_vex.Cross(up);
    l_vex.Normalize();

    Vector3 l_vey = l_vex;
    l_vey.Cross(l_vez);
    l_vey.Normalize();

    mat(0, 0) = l_vex.x;
    mat(0, 1) = l_vex.y;
    mat(0, 2) = l_vex.z;

    mat(1, 0) = l_vey.x;
    mat(1, 1) = l_vey.y;
    mat(1, 2) = l_vey.z;

    mat(2, 0) = l_vez.x;
    mat(2, 1) = l_vez.y;
    mat(2, 2) = l_vez.z;

    return mat;
}

Matrix4 Matrix4::LookAt(const Vector3 &pos, const Vector3 &target, const Vector3 &up)
{
    return Translation(pos * -1) * LookAt(target - pos, up);
}

Matrix4::Matrix4()
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            values[i * 4 + j] = !(j - i);
        }
    }
}

Matrix4::Matrix4(float *v)
{
    memcpy(&values[0], v, sizeof(float) * values.size());
}

Matrix4::Matrix4(const Matrix4 &other)
    : values(other.values)
{
}

float Matrix4::Determinant() const
{
    return At(3, 0) * At(2, 1) * At(1, 2) * At(0, 3) - At(2, 0) * At(3, 1) * At(1, 2) * At(0, 3) - At(3, 0) * At(1, 1)
        * At(2, 2) * At(0, 3) + At(1, 0) * At(3, 1) * At(2, 2) * At(0, 3) + At(2, 0) * At(1, 1) * At(3, 2) * At(0, 3) - At(1, 0)
        * At(2, 1) * At(3, 2) * At(0, 3) - At(3, 0) * At(2, 1) * At(0, 2) * At(1, 3) + At(2, 0) * At(3, 1) * At(0, 2) * At(1, 3)
        + At(3, 0) * At(0, 1) * At(2, 2) * At(1, 3) - At(0, 0) * At(3, 1) * At(2, 2) * At(1, 3) - At(2, 0) * At(0, 1) * At(3, 2)
        * At(1, 3) + At(0, 0) * At(2, 1) * At(3, 2) * At(1, 3) + At(3, 0) * At(1, 1) * At(0, 2) * At(2, 3) - At(1, 0) * At(3, 1)
        * At(0, 2) * At(2, 3) - At(3, 0) * At(0, 1) * At(1, 2) * At(2, 3) + At(0, 0) * At(3, 1) * At(1, 2) * At(2, 3) + At(1, 0)
        * At(0, 1) * At(3, 2) * At(2, 3) - At(0, 0) * At(1, 1) * At(3, 2) * At(2, 3) - At(2, 0) * At(1, 1) * At(0, 2) * At(3, 3)
        + At(1, 0) * At(2, 1) * At(0, 2) * At(3, 3) + At(2, 0) * At(0, 1) * At(1, 2) * At(3, 3) - At(0, 0) * At(2, 1) * At(1, 2)
        * At(3, 3) - At(1, 0) * At(0, 1) * At(2, 2) * At(3, 3) + At(0, 0) * At(1, 1) * At(2, 2) * At(3, 3);
}

Matrix4 &Matrix4::Transpose()
{
    Matrix4 transposed;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            transposed(j, i) = At(i, j);
        }
    }

    return operator=(transposed);
}

Matrix4 &Matrix4::Invert()
{
    float det = Determinant();
    float inv_det = 1.0f / det;

    Matrix4 tmp;

    tmp(0, 0) = At(1, 2) * At(2, 3) * At(3, 1) - At(1, 3) * At(2, 2) * At(3, 1) + At(1, 3) * At(2, 1) * At(3, 2) - At(1, 1)
        * At(2, 3) * At(3, 2) - At(1, 2) * At(2, 1) * At(3, 3) + At(1, 1) * At(2, 2) * At(3, 3);

    tmp(0, 1) = At(0, 3) * At(2, 2) * At(3, 1) - At(0, 2) * At(2, 3) * At(3, 1) - At(0, 3) * At(2, 1) * At(3, 2) + At(0, 1)
        * At(2, 3) * At(3, 2) + At(0, 2) * At(2, 1) * At(3, 3) - At(0, 1) * At(2, 2) * At(3, 3);

    tmp(0, 2) = At(0, 2) * At(1, 3) * At(3, 1) - At(0, 3) * At(1, 2) * At(3, 1) + At(0, 3) * At(1, 1) * At(3, 2) - At(0, 1)
        * At(1, 3) * At(3, 2) - At(0, 2) * At(1, 1) * At(3, 3) + At(0, 1) * At(1, 2) * At(3, 3);

    tmp(0, 3) = At(0, 3) * At(1, 2) * At(2, 1) - At(0, 2) * At(1, 3) * At(2, 1) - At(0, 3) * At(1, 1) * At(2, 2) + At(0, 1)
        * At(1, 3) * At(2, 2) + At(0, 2) * At(1, 1) * At(2, 3) - At(0, 1) * At(1, 2) * At(2, 3);

    tmp(1, 0) = At(1, 3) * At(2, 2) * At(3, 0) - At(1, 2) * At(2, 3) * At(3, 0) - At(1, 3) * At(2, 0) * At(3, 2) + At(1, 0)
        * At(2, 3) * At(3, 2) + At(1, 2) * At(2, 0) * At(3, 3) - At(1, 0) * At(2, 2) * At(3, 3);

    tmp(1, 1) = At(0, 2) * At(2, 3) * At(3, 0) - At(0, 3) * At(2, 2) * At(3, 0) + At(0, 3) * At(2, 0) * At(3, 2) - At(0, 0)
        * At(2, 3) * At(3, 2) - At(0, 2) * At(2, 0) * At(3, 3) + At(0, 0) * At(2, 2) * At(3, 3);

    tmp(1, 2) = At(0, 3) * At(1, 2) * At(3, 0) - At(0, 2) * At(1, 3) * At(3, 0) - At(0, 3) * At(1, 0) * At(3, 2) + At(0, 0)
        * At(1, 3) * At(3, 2) + At(0, 2) * At(1, 0) * At(3, 3) - At(0, 0) * At(1, 2) * At(3, 3);

    tmp(1, 3) = At(0, 2) * At(1, 3) * At(2, 0) - At(0, 3) * At(1, 2) * At(2, 0) + At(0, 3) * At(1, 0) * At(2, 2) - At(0, 0)
        * At(1, 3) * At(2, 2) - At(0, 2) * At(1, 0) * At(2, 3) + At(0, 0) * At(1, 2) * At(2, 3);

    tmp(2, 0) = At(1, 1) * At(2, 3) * At(3, 0) - At(1, 3) * At(2, 1) * At(3, 0) + At(1, 3) * At(2, 0) * At(3, 1) - At(1, 0)
        * At(2, 3) * At(3, 1) - At(1, 1) * At(2, 0) * At(3, 3) + At(1, 0) * At(2, 1) * At(3, 3);

    tmp(2, 1) = At(0, 3) * At(2, 1) * At(3, 0) - At(0, 1) * At(2, 3) * At(3, 0) - At(0, 3) * At(2, 0) * At(3, 1) + At(0, 0)
        * At(2, 3) * At(3, 1) + At(0, 1) * At(2, 0) * At(3, 3) - At(0, 0) * At(2, 1) * At(3, 3);

    tmp(2, 2) = At(0, 1) * At(1, 3) * At(3, 0) - At(0, 3) * At(1, 1) * At(3, 0) + At(0, 3) * At(1, 0) * At(3, 1) - At(0, 0)
        * At(1, 3) * At(3, 1) - At(0, 1) * At(1, 0) * At(3, 3) + At(0, 0) * At(1, 1) * At(3, 3);

    tmp(2, 3) = At(0, 3) * At(1, 1) * At(2, 0) - At(0, 1) * At(1, 3) * At(2, 0) - At(0, 3) * At(1, 0) * At(2, 1) + At(0, 0)
        * At(1, 3) * At(2, 1) + At(0, 1) * At(1, 0) * At(2, 3) - At(0, 0) * At(1, 1) * At(2, 3);

    tmp(3, 0) = At(1, 2) * At(2, 1) * At(3, 0) - At(1, 1) * At(2, 2) * At(3, 0) - At(1, 2) * At(2, 0) * At(3, 1) + At(1, 0)
        * At(2, 2) * At(3, 1) + At(1, 1) * At(2, 0) * At(3, 2) - At(1, 0) * At(2, 1) * At(3, 2);

    tmp(3, 1) = At(0, 1) * At(2, 2) * At(3, 0) - At(0, 2) * At(2, 1) * At(3, 0) + At(0, 2) * At(2, 0) * At(3, 1) - At(0, 0)
        * At(2, 2) * At(3, 1) - At(0, 1) * At(2, 0) * At(3, 2) + At(0, 0) * At(2, 1) * At(3, 2);

    tmp(3, 2) = At(0, 2) * At(1, 1) * At(3, 0) - At(0, 1) * At(1, 2) * At(3, 0) - At(0, 2) * At(1, 0) * At(3, 1) + At(0, 0)
        * At(1, 2) * At(3, 1) + At(0, 1) * At(1, 0) * At(3, 2) - At(0, 0) * At(1, 1) * At(3, 2);

    tmp(3, 3) = At(0, 1) * At(1, 2) * At(2, 0) - At(0, 2) * At(1, 1) * At(2, 0) + At(0, 2) * At(1, 0) * At(2, 1) - At(0, 0)
        * At(1, 2) * At(2, 1) - At(0, 1) * At(1, 0) * At(2, 2) + At(0, 0) * At(1, 1) * At(2, 2);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            At(i, j) = tmp.At(i, j) * inv_det;
        }
    }

    return *this;
}

Matrix4 &Matrix4::operator=(const Matrix4 &other)
{
    values = other.values;
    return *this;
}

Matrix4 Matrix4::operator+(const Matrix4 &other) const
{
    Matrix4 result(*this);
    result += other;

    return result;
}

Matrix4 &Matrix4::operator+=(const Matrix4 &other)
{
    for (int i = 0; i < values.size(); i++) {
        values[i] += other.values[i];
    }

    return *this;
}

Matrix4 Matrix4::operator*(const Matrix4 &other) const
{
    Matrix4 result(Zeroes());

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result.values[i * 4 + j] += values[k * 4 + j] * other.values[i * 4 + k];
            }
        }
    }

    return result;
}

Matrix4 &Matrix4::operator*=(const Matrix4 &other)
{
    (*this) = operator*(other);

    return *this;
}

Matrix4 Matrix4::operator*(float scalar) const
{
    Matrix4 result(*this);
    result *= scalar;

    return result;
}

Matrix4 &Matrix4::operator*=(float scalar)
{
    for (float &value : values) {
        value *= scalar;
    }

    return *this;
}

bool Matrix4::operator==(const Matrix4 &other) const
{
    return values == other.values;
}

Matrix4 Matrix4::Zeroes()
{
    float zero_array[16];
    memset(zero_array, 0, 16 * sizeof(float));
    return Matrix4(zero_array);
}

Matrix4 Matrix4::Ones()
{
    float ones_array[16];
    memset(ones_array, 1, 16 * sizeof(float));
    return Matrix4(ones_array);
}

Matrix4 Matrix4::Identity()
{
    return Matrix4(); // constructor fills out identity matrix
}

std::ostream &operator<<(std::ostream &os, const Matrix4 &mat)
{
    os << "[";
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int index1 = i * 4 + j;
            os << mat.values[index1];
            if (i != 3 && j == 3) {
                os << "\n";
            } else if (!(i == 3 && j == 3)) {
                os << ", ";
            }
        }
    }
    os << "]";
    return os;
}
} // namespace hyperion
