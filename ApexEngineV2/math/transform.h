#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "vector3.h"
#include "quaternion.h"
#include "matrix4.h"
#include "matrix_util.h"

namespace apex {
class Transform {
public:
    Transform();
    Transform(const Vector3 &translation, const Vector3 &scale, const Quaternion &rotation);
    Transform(const Transform &other);

    const Vector3 &GetTranslation() const;
    void SetTranslation(const Vector3 &vec);
    const Vector3 &GetScale() const;
    void SetScale(const Vector3 &vec);
    const Quaternion &GetRotation() const;
    void SetRotation(const Quaternion &rot);
    const Matrix4 &GetMatrix() const;

private:
    Vector3 translation;
    Vector3 scale;
    Quaternion rotation;
    Matrix4 matrix;
    bool update_needed;

    void UpdateMatrix();
};
}

#endif