#ifndef MATRIX4_H
#define MATRIX4_H

#include <iostream>
#include <array>

namespace apex {
class Matrix4 {
    friend std::ostream &operator<<(std::ostream &os, const Matrix4 &mat);
public:
    std::array<float, 16> values;

    Matrix4();
    Matrix4(float *v);
    Matrix4(const Matrix4 &other);

    float Determinant() const;
    Matrix4 &Transpose();
    Matrix4 &Invert();

    Matrix4 &operator=(const Matrix4 &other);
    Matrix4 operator+(const Matrix4 &other) const;
    Matrix4 &operator+=(const Matrix4 &other);
    Matrix4 operator*(const Matrix4 &other) const;
    Matrix4 &operator*=(const Matrix4 &other);
    Matrix4 operator*(float scalar) const;
    Matrix4 &operator*=(float scalar);
    bool operator==(const Matrix4 &other) const;
    float operator()(int i, int j) const;
    float &operator()(int i, int j);

    float At(int i, int j) const;
    float &At(int i, int j);

    static Matrix4 Zeroes();
    static Matrix4 Ones();
    static Matrix4 Identity();
};
} // namespace apex

#endif