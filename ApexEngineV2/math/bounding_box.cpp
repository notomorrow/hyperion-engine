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

std::array<Vector3, 8> BoundingBox::GetCorners() const
{
    std::array<Vector3, 8> corners;
    corners[0] = Vector3(max.x, max.y, max.z);
    corners[1] = Vector3(min.x, max.y, max.z);
    corners[2] = Vector3(min.x, max.y, min.z);
    corners[3] = Vector3(max.x, max.y, min.z);
    corners[4] = Vector3(max.x, min.y, max.z);
    corners[5] = Vector3(min.x, min.y, max.z);
    corners[6] = Vector3(min.x, min.y, min.z);
    corners[7] = Vector3(max.x, min.y, min.z);
    return corners;
}

BoundingBox &BoundingBox::operator*=(double scalar)
{
    min *= scalar;
    max *= scalar;
    return *this;
}

BoundingBox &BoundingBox::operator*=(const Transform &transform)
{
    /*auto corners = GetCorners();

    Clear();

    for (auto &&corner : corners) {
        corner *= mat;
        Extend(corner);
    }*/

    min *= transform.GetScale();
    max *= transform.GetScale();

    min *= transform.GetRotation();
    max *= transform.GetRotation();

    min += transform.GetTranslation();
    max += transform.GetTranslation();

    return *this;
}

BoundingBox &BoundingBox::Clear()
{
    min = Vector3(FLT_MAX);
    max = Vector3(FLT_MIN);
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

bool BoundingBox::IntersectRay(const Ray &ray, Vector3 &out) const
{
    const float epsilon = 1e-6f;

    Vector3 hit_min(FLT_MIN);
    Vector3 hit_max(FLT_MAX);

    if (fabs(ray.direction.x) < epsilon) {
        if (ray.position.x < min.x || ray.position.x > max.x) {
            out = Vector3::Zero();
            return false; // invalid. Doesnt collide
        }
    } else {
        hit_min.x = (min.x - ray.position.x) / ray.direction.x;
        hit_max.x = (max.x - ray.position.x) / ray.direction.x;
        if (hit_min.x > hit_max.x) {
            float temp = hit_min.x;
            hit_min.x = hit_max.x;
            hit_max.x = temp;
        }
    }

    if (fabs(ray.direction.y) < epsilon) {
        if (ray.position.y < min.y ||
            ray.position.y > max.y) {
            out = Vector3::Zero();
            return false; // invalid. Doesnt collide
        }
    } else {
        hit_min.y = (min.y - ray.position.y) / ray.direction.y;
        hit_max.y = (max.y - ray.position.y) / ray.direction.y;

        if (hit_min.y > hit_max.y) {
            float temp = hit_min.y;
            hit_min.y = hit_max.y;
            hit_max.y = temp;
        }

        if (hit_min.x > hit_max.y || hit_min.y > hit_max.x) {
            out = Vector3::Zero();
            return false; // invalid. Doesnt collide
        }

        if (hit_min.y > hit_min.x) hit_min.x = hit_min.y;
        if (hit_max.y < hit_max.x) hit_max.x = hit_max.y;
    }

    if (fabs(ray.direction.z) < epsilon) {
        if (ray.position.z < min.z || ray.position.z > max.z) {
            out = Vector3::Zero();
            return false; // invalid. Doesnt collide
        }
    } else {
        hit_min.z = (min.z - ray.position.z) / ray.direction.z;
        hit_max.z = (max.z - ray.position.z) / ray.direction.z;

        if (hit_min.z > hit_max.z) {
            float temp = hit_min.z;
            hit_min.z = hit_max.z;
            hit_max.z = temp;
        }

        if (hit_min.x > hit_max.z || hit_min.z > hit_max.x) {
            out = Vector3::Zero();
            return false; // invalid. Doesnt collide
        }

        if (hit_min.z > hit_min.x) hit_min.x = hit_min.z;
        if (hit_max.z < hit_max.x) hit_max.x = hit_max.z;
    }

    out = ray.position + (ray.direction * hit_min.x);
    return true;
}

bool BoundingBox::ContainsPoint(const Vector3 &vec) const
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
    Vector3 dimensions(max - min);
    return dimensions.x * dimensions.y * dimensions.z;
}
}