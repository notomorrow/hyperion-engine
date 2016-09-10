#include "matrix3.h"

namespace apex {
Matrix3::Matrix3()
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int index = i * 3 + j;
            values[index] = !(j - i);
        }
    }
}

Matrix3::Matrix3(const float * const v)
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int index = i * 3 + j;
            values[index] = v[index];
        }
    }
}

Matrix3::Matrix3(const Matrix3 &other)
{
    operator=(other);
}

float Matrix3::Determinant() const
{
    float a = (0, 0) * ((1, 1) * (2, 2) - (1, 2) * (2, 1));
    float b = (0, 1) * ((1, 0) * (2, 2) - (1, 2) * (2, 0));
    float c = (0, 2) * ((1, 0) * (2, 1) - (1, 1) * (2, 0));
    return a - b + c;
}

Matrix3 &Matrix3::Transpose()
{
    Matrix3 transposed;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            transposed(j, i) = (i, j);
        }
    }

    return operator=(transposed);
}

Matrix3 &Matrix3::Invert()
{
    float inv_det = 1.0 / Determinant();

    Matrix3 result;
    result(0, 0) = ((1, 1) * (2, 2) - (2, 1) * (1, 2)) * inv_det;
    result(0, 1) = ((0, 2) * (2, 1) - (0, 1) * (2, 2)) * inv_det;
    result(0, 2) = ((0, 1) * (1, 2) - (0, 2) * (1, 1)) * inv_det;
    result(1, 0) = ((1, 2) * (2, 0) - (1, 0) * (2, 2)) * inv_det;
    result(1, 1) = ((0, 0) * (2, 2) - (0, 2) * (2, 0)) * inv_det;
    result(1, 2) = ((1, 0) * (0, 2) - (0, 0) * (1, 2)) * inv_det;
    result(2, 0) = ((1, 0) * (2, 1) - (2, 0) * (1, 1)) * inv_det;
    result(2, 1) = ((2, 0) * (0, 1) - (0, 0) * (2, 1)) * inv_det;
    result(2, 2) = ((0, 0) * (1, 1) - (1, 0) * (0, 1)) * inv_det;

    return operator=(result);
}

Matrix3 &Matrix3::operator=(const Matrix3 &other)
{
    //for (int i = 0; i < 9; i++) {
    //    values[i] = other.values[i];
    //}
    memcpy(values, other.values, 9 * sizeof(float));
    return *this;
}

Matrix3 Matrix3::operator+(const Matrix3 &other) const
{
    Matrix3 result(*this);
    for (int i = 0; i < 9; i++) {
        result.values[i] += other.values[i];
    }
    return result;
}

Matrix3 &Matrix3::operator+=(const Matrix3 &other)
{
    for (int i = 0; i < 9; i++) {
        values[i] += other.values[i];
    }
    return *this;
}

Matrix3 Matrix3::operator*(const Matrix3 &other) const
{
    Matrix3 result(Matrix3::Zeroes());

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                result.values[i * 3 + j] += values[k * 3 + j] * other.values[i * 3 + k];
            }
        }
    }
    return result;
}

Matrix3 &Matrix3::operator*=(const Matrix3 &other)
{
    (*this) = operator*(other);
    return *this;
}

Matrix3 Matrix3::operator*(float scalar) const
{
    Matrix3 result(*this);
    for (int i = 0; i < 9; i++) {
        result.values[i] *= scalar;
    }
    return result;
}

Matrix3 &Matrix3::operator*=(float scalar)
{
    for (int i = 0; i < 9; i++) {
        values[i] *= scalar;
    }
    return *this;
}

bool Matrix3::operator==(const Matrix3 &other) const
{
    for (int i = 0; i < 9; i++) {
        if (values[i] != other.values[i]) {
            return false;
        }
    }
    return true;
}

float Matrix3::operator()(int i, int j) const
{
    return values[i * 3 + j];
}

float &Matrix3::operator()(int i, int j)
{
    return values[i * 3 + j];
}

Matrix3 Matrix3::Zeroes()
{
    float zero_array[9];
    for (int i = 0; i < 9; i++) {
        zero_array[i] = 0.0f;
    }
    return Matrix3(zero_array);
}

Matrix3 Matrix3::Ones()
{
    float ones_array[9];
    for (int i = 0; i < 9; i++) {
        ones_array[i] = 1.0f;
    }
    return Matrix3(ones_array);
}

Matrix3 Matrix3::Identity()
{
    Matrix3 result;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int index = i * 3 + j;
            result.values[index] = !(j - i);
        }
    }
    return result;
}

std::ostream &operator<<(std::ostream &os, const Matrix3 &mat)
{
    os << "[";
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int index1 = i * 3 + j;
            os << mat.values[index1];
            if (i != 2 && j == 2) {
                os << "\n";
            } else if (!(i == 2 && j == 2)) {
                os << ", ";
            }
        }
    }
    os << "]";
    return os;
}
} // namespace apex