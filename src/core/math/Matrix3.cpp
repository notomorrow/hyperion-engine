/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Matrix3.hpp>

#include <core/memory/Memory.hpp>

namespace hyperion {

Matrix3::Matrix3()
    : rows {
          { 1.0f, 0.0f, 0.0f },
          { 0.0f, 1.0f, 0.0f },
          { 0.0f, 0.0f, 1.0f }
      }
{
}

Matrix3::Matrix3(const float* v)
{
    Memory::MemCpy(&values[0], v, std::size(values) * sizeof(values[0]));
}

float Matrix3::Determinant() const
{
    float a = rows[0][0] * (rows[1][1] * rows[2][2] - rows[1][2] * rows[2][1]);
    float b = rows[0][1] * (rows[1][0] * rows[2][2] - rows[1][2] * rows[2][0]);
    float c = rows[0][2] * (rows[1][0] * rows[2][1] - rows[1][1] * rows[2][0]);
    return a - b + c;
}

Matrix3 Matrix3::Transposed() const
{
    const float v[3][3] = {
        { rows[0][0], rows[1][0], rows[2][0] },
        { rows[0][1], rows[1][1], rows[2][1] },
        { rows[0][2], rows[1][2], rows[2][2] }
    };

    return Matrix3(reinterpret_cast<const float*>(v));
}

Matrix3& Matrix3::Transpose()
{
    return *this = Transposed();
}

Matrix3 Matrix3::Inverted() const
{
    const float det = Determinant();
    const float inv_det = 1.0f / det;

    Matrix3 result;
    result[0][0] = (rows[1][1] * rows[2][2] - rows[2][1] * rows[1][2]) * inv_det;
    result[0][1] = (rows[0][2] * rows[2][1] - rows[0][1] * rows[2][2]) * inv_det;
    result[0][2] = (rows[0][1] * rows[1][2] - rows[0][2] * rows[1][1]) * inv_det;
    result[1][0] = (rows[1][2] * rows[2][0] - rows[1][0] * rows[2][2]) * inv_det;
    result[1][1] = (rows[0][0] * rows[2][2] - rows[0][2] * rows[2][0]) * inv_det;
    result[1][2] = (rows[1][0] * rows[0][2] - rows[0][0] * rows[1][2]) * inv_det;
    result[2][0] = (rows[1][0] * rows[2][1] - rows[2][0] * rows[1][1]) * inv_det;
    result[2][1] = (rows[2][0] * rows[0][1] - rows[0][0] * rows[2][1]) * inv_det;
    result[2][2] = (rows[0][0] * rows[1][1] - rows[1][0] * rows[0][1]) * inv_det;

    return result;
}

Matrix3& Matrix3::Invert()
{
    return *this = Inverted();
}

Matrix3 Matrix3::operator+(const Matrix3& other) const
{
    Matrix3 result(*this);

    for (int i = 0; i < std::size(values); i++)
    {
        result.values[i] += other.values[i];
    }

    return result;
}

Matrix3& Matrix3::operator+=(const Matrix3& other)
{
    for (int i = 0; i < std::size(values); i++)
    {
        values[i] += other.values[i];
    }

    return *this;
}

Matrix3 Matrix3::operator*(const Matrix3& other) const
{
    const float fv[] = {
        values[0] * other.values[0] + values[1] * other.values[3] + values[2] * other.values[6],
        values[0] * other.values[1] + values[1] * other.values[4] + values[2] * other.values[7],
        values[0] * other.values[2] + values[1] * other.values[5] + values[2] * other.values[8],

        values[3] * other.values[0] + values[4] * other.values[3] + values[5] * other.values[6],
        values[3] * other.values[1] + values[4] * other.values[4] + values[5] * other.values[7],
        values[3] * other.values[2] + values[4] * other.values[5] + values[5] * other.values[8],

        values[6] * other.values[0] + values[7] * other.values[3] + values[8] * other.values[6],
        values[6] * other.values[1] + values[7] * other.values[4] + values[8] * other.values[7],
        values[6] * other.values[2] + values[7] * other.values[5] + values[8] * other.values[8]
    };

    return Matrix3(fv);
}

Matrix3& Matrix3::operator*=(const Matrix3& other)
{
    return (*this) = operator*(other);
}

Matrix3 Matrix3::operator*(float scalar) const
{
    Matrix3 result(*this);

    for (int i = 0; i < std::size(values); i++)
    {
        result.values[i] *= scalar;
    }

    return result;
}

Matrix3& Matrix3::operator*=(float scalar)
{
    for (int i = 0; i < std::size(values); i++)
    {
        values[i] *= scalar;
    }

    return *this;
}

float Matrix3::operator()(int i, int j) const
{
    return values[i * 3 + j];
}

float& Matrix3::operator()(int i, int j)
{
    return values[i * 3 + j];
}

float Matrix3::At(int i, int j) const
{
    return operator()(i, j);
}

float& Matrix3::At(int i, int j)
{
    return operator()(i, j);
}

Matrix3 Matrix3::Zeros()
{
    float zero_array[sizeof(values) / sizeof(values[0])] = { 0.0f };

    return Matrix3(zero_array);
}

Matrix3 Matrix3::Ones()
{
    float ones_array[sizeof(values) / sizeof(values[0])] = { 1.0f };

    return Matrix3(ones_array);
}

Matrix3 Matrix3::Identity()
{
    return Matrix3(); // constructor fills out identity matrix
}
} // namespace hyperion
