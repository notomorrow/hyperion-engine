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

    inline const Vector3 &GetMin() const { return m_min; }
    inline void SetMin(const Vector3 &min) { m_min = min; }
    inline const Vector3 &GetMax() const { return m_max; }
    inline void SetMax(const Vector3 &max) { m_max = max; }
    inline Vector3 GetDimensions() const { return m_max - m_min; }
    std::array<Vector3, 8> GetCorners() const;

    BoundingBox &operator*=(double scalar);
    BoundingBox &operator*=(const Transform &transform);

    BoundingBox &Clear();
    
    inline bool Empty() const 
    { 
        return m_min == Vector3(std::numeric_limits<float>::max()) && 
               m_max == Vector3(std::numeric_limits<float>::lowest());
    }

    BoundingBox &Extend(const Vector3 &vec);
    BoundingBox &Extend(const BoundingBox &bb);

    bool IntersectRay(const Ray &ray, Vector3 &out) const;
    bool ContainsPoint(const Vector3 &vec) const;
    double Area() const;

private:
    Vector3 m_min;
    Vector3 m_max;
};

} // namespace apex

#endif