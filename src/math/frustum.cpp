#include "frustum.h"

namespace hyperion {
Frustum::Frustum()
{
}

Frustum::Frustum(const Frustum &other)
    : m_planes(other.m_planes)
{
}

Frustum::Frustum(const Matrix4 &view_proj)
{
    SetViewProjectionMatrix(view_proj);
}

bool Frustum::BoundingBoxInFrustum(const BoundingBox &bounding_box) const
{
    const auto &corners = bounding_box.GetCorners();

    for (const auto &plane : m_planes) {
        bool pass = false;

        for (const auto &corner : corners) {
            if (plane.Dot(Vector4(corner.x, corner.y, corner.z, 1.0f)) >= 0.0f) {
                pass = true;
                break;
            }
        }

        if (pass) {
            continue;
        }

        return false;
    }

    return true;
}

void Frustum::SetViewProjectionMatrix(const Matrix4 &view_proj)
{
    Matrix4 mat = view_proj;

    m_planes[0] = (mat.rows[3] + mat.rows[0]).Normalize();
    m_planes[1] = (mat.rows[3] - mat.rows[0]).Normalize();
    m_planes[2] = (mat.rows[3] + mat.rows[1]).Normalize();
    m_planes[3] = (mat.rows[3] - mat.rows[1]).Normalize();
    m_planes[4] = (mat.rows[3] + mat.rows[2]).Normalize();
    m_planes[5] = (mat.rows[3] - mat.rows[2]).Normalize();

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
} // namespace hyperion
