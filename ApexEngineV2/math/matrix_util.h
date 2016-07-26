#ifndef MATRIXUTIL_H
#define MATRIXUTIL_H

#include "quaternion.h"
#include "matrix4.h"
#include "vector3.h"

namespace apex {
class MatrixUtil {
public:
    static void ToTranslation(Matrix4 &mat, const Vector3 &translation);
    static void ToRotation(Matrix4 &mat, const Quaternion &rotation);
    static void ToRotation(Matrix4 &mat, const Vector3 &axis, float radians);
    static void ToScaling(Matrix4 &mat, const Vector3 &scale);
    static void ToPerspective(Matrix4 &mat, float fov, int w, int h, float n, float f);
    static void ToLookAt(Matrix4 &mat, const Vector3 &dir, const Vector3 &up);
    static void ToLookAt(Matrix4 &mat, const Vector3 &pos, const Vector3 &target, const Vector3 &up);
    static void ToOrtho(Matrix4 &mat, float left, float right, float bottom, float top, float n, float f);
};
}
#endif