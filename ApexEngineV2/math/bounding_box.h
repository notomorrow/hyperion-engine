#ifndef BOUNDING_BOX_H
#define BOUNDING_BOX_H

#include "vector3.h"
#include "matrix4.h"
#include "transform.h"
#include "ray.h"
#include <array>

namespace apex {
class BoundingBox {
public:
    BoundingBox();
    BoundingBox(const Vector3 &min, const Vector3 &max);
    BoundingBox(const BoundingBox &other);

    const Vector3 &GetMin() const;
    void SetMin(const Vector3 &);
    const Vector3 &GetMax() const;
    void SetMax(const Vector3 &);
    Vector3 GetDimensions() const;
    std::array<Vector3, 8> GetCorners() const;

    BoundingBox &operator*=(double scalar);
    BoundingBox &operator*=(const Transform &transform);

    BoundingBox &Clear();

    BoundingBox &Extend(const Vector3 &vec);
    BoundingBox &Extend(const BoundingBox &bb);

    bool IntersectRay(const Ray &ray, Vector3 &out) const;
    bool ContainsPoint(const Vector3 &vec) const;
    double Area() const;

private:
    Vector3 min;
    Vector3 max;
};
}

#endif