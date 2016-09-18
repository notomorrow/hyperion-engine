#ifndef QUATERNION_H
#define QUATERNION_H

#include "math_util.h"
#include "vector3.h"
#include "matrix4.h"

#include <cmath>
using std::abs;

namespace apex {
class Quaternion {
    friend std::ostream &operator<<(std::ostream &out, const Quaternion &rot);
public:
    float x, y, z, w;

    Quaternion();
    Quaternion(float x, float y, float z, float w);
    Quaternion(const Matrix4 &mat);
    Quaternion(const Vector3 &euler);
    Quaternion(const Vector3 &axis, float radians);
    Quaternion(const Quaternion &other);

    inline float GetX() const { return x; }
    inline float &GetX() { return x; }
    inline void SetX(float x) { this->x = x; }
    inline float GetY() const { return y; }
    inline float &GetY() { return y; }
    inline void SetY(float y) { this->y = y; }
    inline float GetZ() const { return z; }
    inline float &GetZ() { return z; }
    inline void SetZ(float z) { this->z = z; }
    inline float GetW() const { return w; }
    inline float &GetW() { return w; }
    inline void SetW(float w) { this->w = w; }

    Quaternion &operator=(const Quaternion &other);
    Quaternion operator*(const Quaternion &other) const;
    Quaternion &operator*=(const Quaternion &other);
    Quaternion &operator+=(const Vector3 &vec);
    Vector3 operator*(const Vector3 &vec) const;

    float Length() const;
    float LengthSquared() const;
    Quaternion &Normalize();
    Quaternion &Invert();
    Quaternion &Slerp(const Quaternion &to, float amt);

    int GimbalPole() const;
    float Roll() const;
    float Pitch() const;
    float Yaw() const;

    static Quaternion Identity();
};
} // namespace apex

#endif