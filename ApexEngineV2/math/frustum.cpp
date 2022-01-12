#include "frustum.h"

namespace apex {
enum BoundingBoxFrustumResult {
    OUTSIDE = 0,
    INSIDE = 1,
    INTERSECTS = 2
};
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

    const Vector3 &center = bounding_box.GetCenter();
    const Vector3 &size = bounding_box.GetDimensions();
    unsigned int result = INSIDE; // Assume that the aabb will be inside the frustum

    for (int i = 0; i < 6; i++) {
        const Vector4 &plane = m_planes[i];

        float dot = center.x * plane.x + center.y * plane.y + center.z * plane.z; // dot product
        float radius = size.x * fabsf(plane.x) + size.y * fabsf(plane.y) + size.z * fabsf(plane.z); // radius

        if (dot + radius < -plane.w) {
            result = OUTSIDE;
            break;
        } else if(dot - radius < -plane.w) {
            result = INTERSECTS;
        }
    }

    return result != OUTSIDE;
}

void Frustum::SetViewProjectionMatrix(const Matrix4 &view_proj)
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
