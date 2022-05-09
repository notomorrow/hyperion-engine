#include "bounding_box.h"

#include <limits>
#include <cmath>

namespace hyperion {

BoundingBox::BoundingBox()
    : min(std::numeric_limits<float>::max()), 
      max(std::numeric_limits<float>::lowest())
{
}

BoundingBox::BoundingBox(const Vector3 &min, const Vector3 &max)
    : min(min), 
      max(max)
{
}

BoundingBox::BoundingBox(const BoundingBox &other)
    : min(other.min), 
      max(other.max)
{
}

std::array<Vector3, 8> BoundingBox::GetCorners() const
{
    return std::array<Vector3, 8> {
        Vector3(min.x, min.y, min.z),
        Vector3(max.x, min.y, min.z),
        Vector3(max.x, max.y, min.z),
        Vector3(min.x, max.y, min.z),
        Vector3(min.x, min.y, max.z),
        Vector3(min.x, max.y, max.z),
        Vector3(max.x, max.y, max.z),
        Vector3(max.x, min.y, max.z)
    };
}

void BoundingBox::SetCenter(const Vector3 &center)
{
    Vector3 dimensions = GetDimensions();

    max = center + dimensions * 0.5f;
    min = center - dimensions * 0.5f;
}

void BoundingBox::SetDimensions(const Vector3 &dimensions)
{
    Vector3 center = GetCenter();

    max = center + dimensions * 0.5f;
    min = center - dimensions * 0.5f;
}

BoundingBox &BoundingBox::operator*=(float scalar)
{
    min *= scalar;
    max *= scalar;

    return *this;
}

BoundingBox BoundingBox::operator*(float scalar) const
{
    BoundingBox other;

    for (const Vector3 &corner : GetCorners()) {
        other.Extend(corner * scalar);
    }

    return other;
}

BoundingBox &BoundingBox::operator*=(const Transform &transform)
{
    min *= transform.GetScale();
    max *= transform.GetScale();

    min *= transform.GetRotation();
    max *= transform.GetRotation();

    min += transform.GetTranslation();
    max += transform.GetTranslation();

    return *this;
}

BoundingBox BoundingBox::operator*(const Transform &transform) const
{
    BoundingBox other(*this);

    other.max *= transform.GetMatrix();
    other.min *= transform.GetMatrix();

    return other;
}

BoundingBox &BoundingBox::Clear()
{
    min = Vector3(std::numeric_limits<float>::max());
    max = Vector3(std::numeric_limits<float>::lowest());
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
    min = Vector3::Min(min, bb.min);
    max = Vector3::Max(max, bb.max);
    return *this;
}

bool BoundingBox::IntersectRay(const Ray &ray, RaytestHit &out) const
{
    if (max == Vector3(std::numeric_limits<float>::lowest()) && 
        min == Vector3(std::numeric_limits<float>::max())) {
        // early detect if box is empty
        return false;
    }

    Vector3 hit_min(std::numeric_limits<float>::lowest());
    Vector3 hit_max(std::numeric_limits<float>::max());

    if (std::fabs(ray.m_direction.x) < MathUtil::epsilon<float>) {
        if (ray.m_position.x < min.x || ray.m_position.x > max.x) {
            return false; // no collision
        }
    } else {
        hit_min.x = (min.x - ray.m_position.x) / ray.m_direction.x;
        hit_max.x = (max.x - ray.m_position.x) / ray.m_direction.x;
        if (hit_min.x > hit_max.x) {
            std::swap(hit_min.x, hit_max.x);
        }
    }

    if (std::fabs(ray.m_direction.y) < MathUtil::epsilon<float>) {
        if (ray.m_position.y < min.y ||
            ray.m_position.y > max.y) {
            return false; // no collision
        }
    } else {
        hit_min.y = (min.y - ray.m_position.y) / ray.m_direction.y;
        hit_max.y = (max.y - ray.m_position.y) / ray.m_direction.y;

        if (hit_min.y > hit_max.y) {
            std::swap(hit_min.y, hit_max.y);
        }

        if (hit_min.x > hit_max.y || hit_min.y > hit_max.x) {
            return false; // no collision
        }

        if (hit_min.y > hit_min.x) hit_min.x = hit_min.y;
        if (hit_max.y < hit_max.x) hit_max.x = hit_max.y;
    }

    if (std::fabs(ray.m_direction.z) < MathUtil::epsilon<float>) {
        if (ray.m_position.z < min.z || ray.m_position.z > max.z) {
            return false; // no collision
        }
    } else {
        hit_min.z = (min.z - ray.m_position.z) / ray.m_direction.z;
        hit_max.z = (max.z - ray.m_position.z) / ray.m_direction.z;

        if (hit_min.z > hit_max.z) {
            std::swap(hit_min.z, hit_max.z);
        }

        if (hit_min.x > hit_max.z || hit_min.z > hit_max.x) {
            return false; // no collision
        }

        if (hit_min.z > hit_min.x) hit_min.x = hit_min.z;
        if (hit_max.z < hit_max.x) hit_max.x = hit_max.z;
    }

    out.hitpoint = ray.m_position + (ray.m_direction * hit_min.x);

    return true;
}

bool BoundingBox::Intersects(const BoundingBox &other) const
{
    for (const auto &corner : other.GetCorners()) {
        if (ContainsPoint(corner)) {
            return true;
        }
    }

    return false;
}

bool BoundingBox::Contains(const BoundingBox &other) const
{
    for (const auto &corner : other.GetCorners()) {
        if (!ContainsPoint(corner)) {
            return false;
        }
    }

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

std::ostream &operator<<(std::ostream &out, const BoundingBox &bb) // output
{
    out << "BoundingBox [max: " << bb.GetMax() << ", min: " << bb.GetMin() << "]";
    return out;
}

} // namespace hyperion
