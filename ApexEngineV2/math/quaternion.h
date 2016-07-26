#ifndef QUATERNION_H
#define QUATERNION_H

#include "math_util.h"
#include "vector3.h"
#include "matrix4.h"

#include <cmath>
using std::abs;

namespace apex {
class Quaternion {
public:
    float x, y, z, w;

    Quaternion();
    Quaternion(float x, float y, float z, float w);
    Quaternion(const Matrix4 &mat);
    Quaternion(const Vector3 &axis, float radians);
    Quaternion(const Quaternion &other);

    Quaternion &operator=(const Quaternion &other);
    Quaternion operator*(const Quaternion &other) const;
    Quaternion &operator*=(const Quaternion &other);
    Vector3 operator*(const Vector3 &vec) const;

    float Length() const;
    Quaternion &Invert();
    Quaternion &Slerp(const Quaternion &to, float amt);

    int GimbalPole() const;
    float Roll() const;
    float Pitch() const;
    float Yaw() const;

    static Quaternion Identity();

    friend std::ostream &operator<<(std::ostream &out, const Quaternion &rot) // output
    {
        out << "[" << rot.x << ", " << rot.y << ", " << rot.z << ", " << rot.w << "]";
        return out;
    }
};
}

#endif