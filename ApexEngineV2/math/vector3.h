#ifndef VECTOR3_H
#define VECTOR3_H

#include <cmath>
#include <iostream>

#include "math_util.h"
#include "matrix4.h"

namespace apex {
class Quaternion;
class Vector3 {
public:
    float x, y, z;

    Vector3();
    Vector3(float x, float y, float z);
    Vector3(float xyz);
    Vector3(const Vector3 &other);

    Vector3 &operator=(const Vector3 &other);
    Vector3 operator+(const Vector3 &other) const;
    Vector3 &operator+=(const Vector3 &other);
    Vector3 operator-(const Vector3 &other) const;
    Vector3 &operator-=(const Vector3 &other);
    Vector3 operator*(const Vector3 &other) const;
    Vector3 &operator*=(const Vector3 &other);
    Vector3 operator*(const Matrix4 &mat) const;
    Vector3 &operator*=(const Matrix4 &mat);
    Vector3 operator*(const Quaternion &quat) const;
    Vector3 &operator*=(const Quaternion &quat);
    Vector3 operator/(const Vector3 &other) const;
    Vector3 &operator/=(const Vector3 &other);
    bool operator==(const Vector3 &other) const;
    bool operator!=(const Vector3 &other) const;

    float Length() const;
    Vector3 &Normalize();
    Vector3 &Cross(const Vector3 &other);
    Vector3 &Rotate(const Vector3 &axis, float radians);
    Vector3 &Lerp(const Vector3 &to, const float amt);

    float Dot(const Vector3 &other) const;
    float DistanceSquared(const Vector3 &other) const;
    float Distance(const Vector3 &other) const;

    static Vector3 Abs(const Vector3 &);
    static Vector3 Round(const Vector3 &);
    static Vector3 Clamp(const Vector3 &, float min, float max);
    static Vector3 Min(const Vector3 &a, const Vector3 &b);
    static Vector3 Max(const Vector3 &a, const Vector3 &b);

    static Vector3 Zero();
    static Vector3 One();
    static Vector3 UnitX();
    static Vector3 UnitY();
    static Vector3 UnitZ();

    friend std::ostream &operator<<(std::ostream &out, const Vector3 &vec);
};
}
#endif
