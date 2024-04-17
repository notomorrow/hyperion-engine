/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_BOUNDING_SPHERE_H
#define HYPERION_BOUNDING_SPHERE_H

#include <math/Vector3.hpp>
#include <math/Matrix4.hpp>
#include <math/BoundingBox.hpp>
#include <math/Transform.hpp>
#include <math/Ray.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <array>
#include <limits>

namespace hyperion {

struct HYP_API BoundingSphere
{
public:
    static const BoundingSphere empty;
    static const BoundingSphere infinity;

    BoundingSphere();
    BoundingSphere(const Vec3f &center, float radius);
    BoundingSphere(const BoundingSphere &other);
    BoundingSphere &operator=(const BoundingSphere &other);
    BoundingSphere(BoundingSphere &&other) noexcept;
    BoundingSphere &operator=(BoundingSphere &&other) noexcept;
    BoundingSphere(const BoundingBox &box);

    const Vec3f &GetCenter() const
        { return center; }

    void SetCenter(const Vec3f &center)
        { this->center = center; }

    float GetRadius() const
        { return radius; }

    void SetRadius(float radius)
        { this->radius = radius; }

    bool operator==(const BoundingSphere &other) const
        { return center == other.center && radius == other.radius; }

    bool operator!=(const BoundingSphere &other) const
        { return !operator==(other); }

    BoundingSphere &Extend(const BoundingBox &box);

    /*! \brief Convert the BoundingSphere to an AABB. */
    operator BoundingBox() const
        { return BoundingBox(center - Vec3f(radius), center + Vec3f(radius)); }

    /*! \brief Store the BoundingSphere in a Vector4.
        x,y,z components will be the center of the sphere,
        w will be the radius. */
    Vec4f ToVector4() const;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(center.GetHashCode());
        hc.Add(radius);

        return hc;
    }

    Vec3f   center;
    float   radius;
};

} // namespace hyperion

#endif
