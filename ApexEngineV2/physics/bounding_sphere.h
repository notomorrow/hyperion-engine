#ifndef BOUNDING_SPHERE_H
#define BOUNDING_SPHERE_H

#include "contact.h"
#include "../math/vector3.h"

namespace apex {
class BoundingSphere {
public:
    BoundingSphere(const Vector3 &center, double radius);
    BoundingSphere(const BoundingSphere &one, const BoundingSphere &two);

    Vector3 GetCenter() const;
    void SetCenter(const Vector3 &center);
    double GetRadius() const;
    void SetRadius(double radius);

    double GetVolume() const;

    bool Overlaps(const BoundingSphere &other) const;

protected:
    Vector3 m_center;
    double m_radius;
};
}

#endif