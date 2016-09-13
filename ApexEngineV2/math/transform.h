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

    inline const Vector3 &GetTranslation() const { return translation; }
    /** returns a reference to the translation - if modified, you must call UpdateMatrix(). */
    inline Vector3 &GetTranslation() { return translation; }
    inline void SetTranslation(const Vector3 &vec) { translation = vec; UpdateMatrix(); }

    inline const Vector3 &GetScale() const { return scale; }
    inline Vector3 &GetScale() { return scale; }
    /** returns a reference to the scale - if modified, you must call UpdateMatrix(). */
    inline void SetScale(const Vector3 &vec) { scale = vec; UpdateMatrix(); }

    inline const Quaternion &GetRotation() const { return rotation; }
    /** returns a reference to the rotation - if modified, you must call UpdateMatrix(). */
    inline Quaternion &GetRotation() { return rotation; }
    inline void SetRotation(const Quaternion &rot) { rotation = rot; UpdateMatrix(); }

    void UpdateMatrix();
    inline const Matrix4 &GetMatrix() const { return matrix; }

private:
    Vector3 translation;
    Vector3 scale;
    Quaternion rotation;
    Matrix4 matrix;
};
} // namespace apex

#endif