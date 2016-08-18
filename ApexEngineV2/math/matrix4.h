#ifndef MATRIX4_H
#define MATRIX4_H

#include <iostream>

namespace apex {
struct Matrix4 {
    float values[16];

    Matrix4();
    Matrix4(const float * const v);
    Matrix4(const Matrix4 &other);

    float Determinant() const;
    Matrix4 &Transpose();
    Matrix4 &Invert();

    Matrix4 &operator=(const Matrix4 &other);
    Matrix4 operator*(const Matrix4 &other) const;
    Matrix4 &operator*=(const Matrix4 &other);
    bool operator==(const Matrix4 &other) const;
    float operator()(int i, int j) const;
    float &operator()(int i, int j);

    static Matrix4 Zeroes();
    static Matrix4 Ones();
    static Matrix4 Identity();

    friend std::ostream &operator<<(std::ostream &os, const Matrix4 &mat);
};
} // namespace apex

#endif