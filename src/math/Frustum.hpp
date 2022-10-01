#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "Matrix4.hpp"
#include "Vector4.hpp"
#include "BoundingBox.hpp"

#include <core/lib/FixedArray.hpp>

#include <Types.hpp>

namespace hyperion {
class Frustum
{
public:
    Frustum();
    Frustum(const Frustum &other);
    Frustum(const Matrix4 &view_proj);

    FixedArray<Vector4, 6> &GetPlanes() { return m_planes; }
    const FixedArray<Vector4, 6> &GetPlanes() const { return m_planes; }

    Vector4 &GetPlane(UInt index) { return m_planes[index]; }
    const Vector4 &GetPlane(UInt index) const { return m_planes[index]; }

    bool ContainsAABB(const BoundingBox &aabb) const;

    Frustum &SetFromViewProjectionMatrix(const Matrix4 &view_proj);
    //Frustum &SetFromAABB(const BoundingBox &aabb);
    //Frustum &SetFromCorners(const std::array<Vector3, 8> &corners);

    FixedArray<Vector4, 6> m_planes;
};
} // namespace hyperion

#endif

