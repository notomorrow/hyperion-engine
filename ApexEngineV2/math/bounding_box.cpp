#include "bounding_box.h"

namespace apex {
BoundingBox::BoundingBox()
    : min(FLT_MAX), max(FLT_MIN)
{
}

BoundingBox::BoundingBox(const Vector3 &min, const Vector3 &max)
    : min(min), max(max)
{
}

BoundingBox::BoundingBox(const BoundingBox &other)
    : min(other.min), max(other.max)
{
}

const Vector3 &BoundingBox::GetMin() const
{
    return min;
}

void BoundingBox::SetMin(const Vector3 &vec)
{
    min = vec;
}

const Vector3 &BoundingBox::GetMax() const
{
    return max;
}

void BoundingBox::SetMax(const Vector3 &vec)
{
    max = vec;
}

Vector3 BoundingBox::GetDimensions() const
{
    return max - min;
}

BoundingBox &BoundingBox::operator*=(double scalar)
{
    min *= scalar;
    max *= scalar;
    return *this;
}

BoundingBox &BoundingBox::operator*=(const Matrix4 &mat)
{
    min *= mat;
    max *= mat;
    return *this;
}

BoundingBox &BoundingBox::Extend(const Vector3 &vec)
{
    min = Vector3::Min(min, vec);
    max = Vector3::Max(max, vec);
    return *this;
}

BoundingBox &BoundingBox::Extend(const BoundingBox &bb)
{
    Extend(bb.min);
    Extend(bb.max);
    return *this;
}

bool BoundingBox::Contains(const Vector3 &vec) const
{
    if (vec.x < min.x) return false;
    if (vec.y < min.y) return false;
    if (vec.z < min.z) return false;
    if (vec.x > max.x) return false;
    if (vec.y > max.y) return false;
    if (vec.z > max.z) return false;
    return true;
}

double BoundingBox::Area() const
{
    Vector3 dimensions = max - min;
    return dimensions.x * dimensions.y * dimensions.z;
}
}