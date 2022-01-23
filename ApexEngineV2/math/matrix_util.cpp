#include "matrix_util.h"

namespace hyperion {
void MatrixUtil::ToTranslation(Matrix4 &mat, const Vector3 &translation)
{
    mat = Matrix4::Identity();

    mat(0, 3) = translation.x;
    mat(1, 3) = translation.y;
    mat(2, 3) = translation.z;
}

Vector3 MatrixUtil::ExtractTranslation(const Matrix4 &mat)
{
    return Vector3(mat(0, 3), mat(1, 3), mat(2, 3));
}

void MatrixUtil::ToRotation(Matrix4 &mat, const Quaternion &rotation)
{
    float xx = rotation.x * rotation.x;
    float xy = rotation.x * rotation.y;
    float xz = rotation.x * rotation.z;
    float xw = rotation.x * rotation.w;
    float yy = rotation.y * rotation.y;
    float yz = rotation.y * rotation.z;
    float yw = rotation.y * rotation.w;
    float zz = rotation.z * rotation.z;
    float zw = rotation.z * rotation.w;

    mat(0, 0) = 1.0f - 2.0f * (yy + zz);
    mat(0, 1) = 2.0f * (xy + zw);
    mat(0, 2) = 2.0f * (xz - yw);
    mat(0, 3) = 0.0;

    mat(1, 0) = 2.0f * (xy - zw);
    mat(1, 1) = 1.0f - 2.0f * (xx + zz);
    mat(1, 2) = 2.0f * (yz + xw);
    mat(1, 3) = 0.0;

    mat(2, 0) = 2.0f * (xz + yw);
    mat(2, 1) = 2.0f * (yz - xw);
    mat(2, 2) = 1.0f - 2.0f * (xx + yy);
    mat(2, 3) = 0.0;

    mat(3, 0) = 0.0f;
    mat(3, 1) = 0.0;
    mat(3, 2) = 0.0;
    mat(3, 3) = 1.0;
}

void MatrixUtil::ToRotation(Matrix4 &mat, const Vector3 &axis, float radians)
{
    mat = Matrix4::Identity();

    ToRotation(mat, Quaternion(axis, radians));
}

void MatrixUtil::ToScaling(Matrix4 &mat, const Vector3 &scale)
{
    mat = Matrix4::Identity();

    mat(0, 0) = scale.x;
    mat(1, 1) = scale.y;
    mat(2, 2) = scale.z;
}

void MatrixUtil::ToPerspective(Matrix4 &mat, float fov, int w, int h, float n, float f)
{
    mat = Matrix4::Identity();

    float ar = (float)w / (float)h;
    float tan_half_fov = tan(MathUtil::DegToRad(fov / 2.0f));
    float range = n - f;

    mat(0, 0) = 1.0f / (tan_half_fov * ar);
    mat(0, 1) = 0.0f;
    mat(0, 2) = 0.0f;
    mat(0, 3) = 0.0f;

    mat(1, 0) = 0.0f;
    mat(1, 1) = 1.0f / (tan_half_fov);
    mat(1, 2) = 0.0f;
    mat(1, 3) = 0.0f;

    mat(2, 0) = 0.0f;
    mat(2, 1) = 0.0f;
    mat(2, 2) = (-n - f) / range;
    mat(2, 3) = (2.0f * f * n) / range;

    mat(3, 0) = 0.0f;
    mat(3, 1) = 0.0f;
    mat(3, 2) = 1.0f;
    mat(3, 3) = 0.0f;
}

void MatrixUtil::ToLookAt(Matrix4 &mat, const Vector3 &dir, const Vector3 &up)
{
    mat = Matrix4::Identity();

    Vector3 l_vez = dir;
    l_vez.Normalize();

    Vector3 l_vex = dir;
    l_vex.Normalize();
    l_vex.Cross(up);
    l_vex.Normalize();

    Vector3 l_vey = l_vex;
    l_vey.Cross(l_vez);
    l_vey.Normalize();

    mat(0, 0) = l_vex.x;
    mat(0, 1) = l_vex.y;
    mat(0, 2) = l_vex.z;

    mat(1, 0) = l_vey.x;
    mat(1, 1) = l_vey.y;
    mat(1, 2) = l_vey.z;

    mat(2, 0) = l_vez.x;
    mat(2, 1) = l_vez.y;
    mat(2, 2) = l_vez.z;
}

void MatrixUtil::ToLookAt(Matrix4 &mat, const Vector3 &pos, const Vector3 &target, const Vector3 &up)
{
    Matrix4 trans, rot;
    ToTranslation(trans, pos * -1);
    ToLookAt(rot, target - pos, up);

    mat = trans * rot;
}

void MatrixUtil::ToOrtho(Matrix4 &mat, 
    float left, float right, 
    float bottom, float top, 
    float near_clip, float far_clip)
{
    float x_orth = 2.0f / (right - left);
    float y_orth = 2.0f / (top - bottom);
    float z_orth = 2.0f / (far_clip - near_clip);
    float tx = -1 * ((right + left) / (right - left));
    float ty = -1 * ((top + bottom) / (top - bottom));
    float tz = -1 * ((far_clip + near_clip) / (far_clip - near_clip));

    mat(0, 0) = x_orth;
    mat(0, 1) = 0;
    mat(0, 2) = 0;
    mat(0, 3) = tx;

    mat(1, 0) = 0;
    mat(1, 1) = y_orth;
    mat(1, 2) = 0;
    mat(1, 3) = ty;

    mat(2, 0) = 0;
    mat(2, 1) = 0;
    mat(2, 2) = z_orth;
    mat(2, 3) = tz;

    mat(3, 0) = 0;
    mat(3, 1) = 0;
    mat(3, 2) = 0;
    mat(3, 3) = 1;
}

Matrix3 MatrixUtil::CreateInertiaTensor(const Vector3 &half_size, double mass)
{
    Matrix3 res = Matrix3::Zeroes();
    Vector3 sqr = half_size * half_size;
    res.values[0] = 0.3 * mass * (sqr.GetY() + sqr.GetZ());
    res.values[4] = 0.3 * mass * (sqr.GetX() + sqr.GetZ());
    res.values[8] = 0.3 * mass * (sqr.GetX() + sqr.GetY());
    return res;
}
} // namespace hyperion
