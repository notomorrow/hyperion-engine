#include "matrix4.h"
#include <cassert>

namespace apex {
Matrix4::Matrix4()
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int index = i * 4 + j;
            values[index] = !(j - i);
        }
    }
}

Matrix4::Matrix4(float *v)
{
    memcpy(values, v, sizeof(float) * 16);
}

Matrix4::Matrix4(const Matrix4 &other)
{
    operator=(other);
}

float Matrix4::Determinant() const
{
    float l_det =
        At(3, 0) * At(2, 1) * At(1, 2) * At(0, 3) - At(2, 0) * At(3, 1) * At(1, 2) * At(0, 3) - At(3, 0) * At(1, 1)
        * At(2, 2) * At(0, 3) + At(1, 0) * At(3, 1) * At(2, 2) * At(0, 3) + At(2, 0) * At(1, 1) * At(3, 2) * At(0, 3) - At(1, 0)
        * At(2, 1) * At(3, 2) * At(0, 3) - At(3, 0) * At(2, 1) * At(0, 2) * At(1, 3) + At(2, 0) * At(3, 1) * At(0, 2) * At(1, 3)
        + At(3, 0) * At(0, 1) * At(2, 2) * At(1, 3) - At(0, 0) * At(3, 1) * At(2, 2) * At(1, 3) - At(2, 0) * At(0, 1) * At(3, 2)
        * At(1, 3) + At(0, 0) * At(2, 1) * At(3, 2) * At(1, 3) + At(3, 0) * At(1, 1) * At(0, 2) * At(2, 3) - At(1, 0) * At(3, 1)
        * At(0, 2) * At(2, 3) - At(3, 0) * At(0, 1) * At(1, 2) * At(2, 3) + At(0, 0) * At(3, 1) * At(1, 2) * At(2, 3) + At(1, 0)
        * At(0, 1) * At(3, 2) * At(2, 3) - At(0, 0) * At(1, 1) * At(3, 2) * At(2, 3) - At(2, 0) * At(1, 1) * At(0, 2) * At(3, 3)
        + At(1, 0) * At(2, 1) * At(0, 2) * At(3, 3) + At(2, 0) * At(0, 1) * At(1, 2) * At(3, 3) - At(0, 0) * At(2, 1) * At(1, 2)
        * At(3, 3) - At(1, 0) * At(0, 1) * At(2, 2) * At(3, 3) + At(0, 0) * At(1, 1) * At(2, 2) * At(3, 3);
    return l_det;
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
    assert(det != 0.0f);

    float inv_det = 1.0 / det;

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
    memcpy(values, other.values, 16 * sizeof(float));
    return *this;
}

Matrix4 Matrix4::operator+(const Matrix4 &other) const
{
    Matrix4 result(*this);
    for (int i = 0; i < 16; i++) {
        result.values[i] += other.values[i];
    }
    return result;
}

Matrix4 &Matrix4::operator+=(const Matrix4 &other)
{
    for (int i = 0; i < 16; i++) {
        values[i] += other.values[i];
    }
    return *this;
}

Matrix4 Matrix4::operator*(const Matrix4 &other) const
{
    Matrix4 result(Matrix4::Zeroes());

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
    for (int i = 0; i < 16; i++) {
        result.values[i] *= scalar;
    }
    return result;
}

Matrix4 &Matrix4::operator*=(float scalar)
{
    for (int i = 0; i < 16; i++) {
        values[i] *= scalar;
    }
    return *this;
}

bool Matrix4::operator==(const Matrix4 &other) const
{
    for (int i = 0; i < 16; i++) {
        if (values[i] != other.values[i]) {
            return false;
        }
    }
    return true;
}

float Matrix4::operator()(int i, int j) const
{
    return values[i * 4 + j];
}

float &Matrix4::operator()(int i, int j)
{
    return values[i * 4 + j];
}

float Matrix4::At(int i, int j) const
{
    return operator()(i, j);
}

float &Matrix4::At(int i, int j)
{
    return operator()(i, j);
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
    Matrix4 result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int index = i * 4 + j;
            result.values[index] = !(j - i);
        }
    }
    return result;
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
} // namespace apex