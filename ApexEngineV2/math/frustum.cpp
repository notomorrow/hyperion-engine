#include "frustum.h"

namespace apex {
Frustum::Frustum(const Matrix4 &view_proj)
{
    Matrix4 mat = view_proj;

    Vector4 rows[6];
    for (int i = 0; i < 4; i++) {
        rows[i].x = mat(i, 0);
        rows[i].y = mat(i, 1);
        rows[i].z = mat(i, 2);
        rows[i].w = mat(i, 3);
    }

    planes[0] = (rows[3] + rows[0]).Normalize();
    planes[1] = (rows[3] - rows[0]).Normalize();
    planes[2] = (rows[3] + rows[1]).Normalize();
    planes[3] = (rows[3] - rows[1]).Normalize();
    planes[4] = (rows[3] + rows[2]).Normalize();
    planes[5] = (rows[3] - rows[2]).Normalize();

    mat.Transpose();
    
    planes[0].x = mat.values[3] - mat.values[0];
    planes[0].y = mat.values[7] - mat.values[4];
    planes[0].z = mat.values[11] - mat.values[8];
    planes[0].w = mat.values[15] - mat.values[12];
    planes[0].Normalize();

    planes[1].x = mat.values[3] + mat.values[0];
    planes[1].y = mat.values[7] + mat.values[4];
    planes[1].z = mat.values[11] + mat.values[8];
    planes[1].w = mat.values[15] + mat.values[12];
    planes[1].Normalize();

    planes[2].x = mat.values[3] + mat.values[1];
    planes[2].y = mat.values[7] + mat.values[5];
    planes[2].z = mat.values[11] + mat.values[9];
    planes[2].w = mat.values[15] + mat.values[13];
    planes[2].Normalize();

    planes[3].x = mat.values[3] - mat.values[1];
    planes[3].y = mat.values[7] - mat.values[5];
    planes[3].z = mat.values[11] - mat.values[9];
    planes[3].w = mat.values[15] - mat.values[13];
    planes[3].Normalize();

    planes[4].x = mat.values[3] - mat.values[2];
    planes[4].y = mat.values[7] - mat.values[6];
    planes[4].z = mat.values[11] - mat.values[10];
    planes[4].w = mat.values[15] - mat.values[14];
    planes[4].Normalize();

    planes[5].x = mat.values[3] + mat.values[2];
    planes[5].y = mat.values[7] + mat.values[6];
    planes[5].z = mat.values[11] + mat.values[10];
    planes[5].w = mat.values[15] + mat.values[14];
    planes[5].Normalize();
}

Vector4 &Frustum::GetPlane(size_t index)
{
    return planes[index];
}

const Vector4 &Frustum::GetPlane(size_t index) const
{
    return planes[index];
}
}
