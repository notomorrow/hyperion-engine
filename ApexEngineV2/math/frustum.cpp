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

    m_planes[0] = (rows[3] + rows[0]).Normalize();
    m_planes[1] = (rows[3] - rows[0]).Normalize();
    m_planes[2] = (rows[3] + rows[1]).Normalize();
    m_planes[3] = (rows[3] - rows[1]).Normalize();
    m_planes[4] = (rows[3] + rows[2]).Normalize();
    m_planes[5] = (rows[3] - rows[2]).Normalize();

    mat.Transpose();
    
    m_planes[0].x = mat.values[3] - mat.values[0];
    m_planes[0].y = mat.values[7] - mat.values[4];
    m_planes[0].z = mat.values[11] - mat.values[8];
    m_planes[0].w = mat.values[15] - mat.values[12];
    m_planes[0].Normalize();

    m_planes[1].x = mat.values[3] + mat.values[0];
    m_planes[1].y = mat.values[7] + mat.values[4];
    m_planes[1].z = mat.values[11] + mat.values[8];
    m_planes[1].w = mat.values[15] + mat.values[12];
    m_planes[1].Normalize();

    m_planes[2].x = mat.values[3] + mat.values[1];
    m_planes[2].y = mat.values[7] + mat.values[5];
    m_planes[2].z = mat.values[11] + mat.values[9];
    m_planes[2].w = mat.values[15] + mat.values[13];
    m_planes[2].Normalize();

    m_planes[3].x = mat.values[3] - mat.values[1];
    m_planes[3].y = mat.values[7] - mat.values[5];
    m_planes[3].z = mat.values[11] - mat.values[9];
    m_planes[3].w = mat.values[15] - mat.values[13];
    m_planes[3].Normalize();

    m_planes[4].x = mat.values[3] - mat.values[2];
    m_planes[4].y = mat.values[7] - mat.values[6];
    m_planes[4].z = mat.values[11] - mat.values[10];
    m_planes[4].w = mat.values[15] - mat.values[14];
    m_planes[4].Normalize();

    m_planes[5].x = mat.values[3] + mat.values[2];
    m_planes[5].y = mat.values[7] + mat.values[6];
    m_planes[5].z = mat.values[11] + mat.values[10];
    m_planes[5].w = mat.values[15] + mat.values[14];
    m_planes[5].Normalize();
}
} // namespace apex
