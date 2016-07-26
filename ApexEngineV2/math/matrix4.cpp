#include "matrix4.h"

namespace apex {
Matrix4::Matrix4()
{
    int offset = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int index = i * 4 + j;
            values[index] = !(j - i);
        }
    }
}

Matrix4::Matrix4(const float * const v)
{
    for (int i = 0; i < 16; i++) {
        values[i] = v[i];
    }
}

Matrix4::Matrix4(const Matrix4 &other)
{
    for (int i = 0; i < 16; i++) {
        values[i] = other.values[i];
    }
}

Matrix4 &Matrix4::operator=(const Matrix4 &other)
{
    for (int i = 0; i < 16; i++) {
        values[i] = other.values[i];
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
    int index = i * 4 + j;
    return values[index];
}

float &Matrix4::operator()(int i, int j)
{
    int index = i * 4 + j;
    return values[index];
}

Matrix4 Matrix4::Zeroes()
{
    float zero_array[16];
    for (int i = 0; i < 16; i++) {
        zero_array[i] = 0.0;
    }
    return Matrix4(zero_array);
}

Matrix4 Matrix4::Identity()
{
    Matrix4 result;
    int offset = 0;
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