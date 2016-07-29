#ifndef BOUNDING_BOX_H
#define BOUNDING_BOX_H

#include "vector3.h"
#include "matrix4.h"

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

    BoundingBox &operator*=(double scalar);
    BoundingBox &operator*=(const Matrix4 &mat);

    BoundingBox &Extend(const Vector3 &vec);
    BoundingBox &Extend(const BoundingBox &bb);

    bool Contains(const Vector3 &vec) const;
    double Area() const;

private:
    Vector3 min;
    Vector3 max;
};
}

#endif