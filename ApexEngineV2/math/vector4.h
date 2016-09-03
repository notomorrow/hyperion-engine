#ifndef VECTOR4_H
#define VECTOR4_H

#include <cmath>
#include <iostream>
#include "math_util.h"
#include "matrix4.h"

namespace apex {
class Vector3;
class Vector4 {
public:
    float x, y, z, w;

    Vector4();
    Vector4(float x, float y, float z, float w);
    Vector4(float xyzw);
    Vector4(const Vector4 &other);

    Vector4 &operator=(const Vector4 &other);
    Vector4 operator+(const Vector4 &other) const;
    Vector4 &operator+=(const Vector4 &other);
    Vector4 operator-(const Vector4 &other) const;
    Vector4 &operator-=(const Vector4 &other);
    Vector4 operator*(const Vector4 &other) const;
    Vector4 &operator*=(const Vector4 &other);
    Vector4 operator*(const Matrix4 &mat) const;
    Vector4 &operator*=(const Matrix4 &mat);
    Vector4 operator/(const Vector4 &other) const;
    Vector4 &operator/=(const Vector4 &other);
    bool operator==(const Vector4 &other) const;
    bool operator!=(const Vector4 &other) const;

    float Length() const;
    Vector4 &Normalize();
    Vector4 &Rotate(const Vector3 &axis, float radians);
    Vector4 &Lerp(const Vector4 &to, const float amt);

    float Dot(const Vector4 &other) const;
    float DistanceSquared(const Vector4 &other) const;
    float Distance(const Vector4 &other) const;

    static Vector4 Abs(const Vector4 &);
    static Vector4 Round(const Vector4 &);
    static Vector4 Clamp(const Vector4 &, float min, float max);
    static Vector4 Min(const Vector4 &a, const Vector4 &b);
    static Vector4 Max(const Vector4 &a, const Vector4 &b);

    static Vector4 Zero();
    static Vector4 One();
    static Vector4 UnitX();
    static Vector4 UnitY();
    static Vector4 UnitZ();
    static Vector4 UnitW();

    friend std::ostream &operator<<(std::ostream &out, const Vector4 &vec);
};
}
#endif
