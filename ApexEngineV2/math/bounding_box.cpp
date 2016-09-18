#include "bounding_box.h"

namespace apex {
BoundingBox::BoundingBox()
    : m_min(FLT_MAX), m_max(FLT_MIN)
{
}

BoundingBox::BoundingBox(const Vector3 &min, const Vector3 &max)
    : m_min(min), m_max(max)
{
}

BoundingBox::BoundingBox(const BoundingBox &other)
    : m_min(other.m_min), m_max(other.m_max)
{
}

std::array<Vector3, 8> BoundingBox::GetCorners() const
{
    std::array<Vector3, 8> corners;
    corners[0] = Vector3(m_max.x, m_max.y, m_max.z);
    corners[1] = Vector3(m_min.x, m_max.y, m_max.z);
    corners[2] = Vector3(m_min.x, m_max.y, m_min.z);
    corners[3] = Vector3(m_max.x, m_max.y, m_min.z);
    corners[4] = Vector3(m_max.x, m_min.y, m_max.z);
    corners[5] = Vector3(m_min.x, m_min.y, m_max.z);
    corners[6] = Vector3(m_min.x, m_min.y, m_min.z);
    corners[7] = Vector3(m_max.x, m_min.y, m_min.z);
    return corners;
}

BoundingBox &BoundingBox::operator*=(double scalar)
{
    m_min *= scalar;
    m_max *= scalar;
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

    m_min *= transform.GetScale();
    m_max *= transform.GetScale();

    m_min *= transform.GetRotation();
    m_max *= transform.GetRotation();

    m_min += transform.GetTranslation();
    m_max += transform.GetTranslation();

    return *this;
}

BoundingBox &BoundingBox::Clear()
{
    m_min = Vector3(FLT_MAX);
    m_max = Vector3(FLT_MIN);
    return *this;
}

BoundingBox &BoundingBox::Extend(const Vector3 &vec)
{
    m_min = Vector3::Min(m_min, vec);
    m_max = Vector3::Max(m_max, vec);
    return *this;
}

BoundingBox &BoundingBox::Extend(const BoundingBox &bb)
{
    Extend(bb.m_min);
    Extend(bb.m_max);
    return *this;
}

bool BoundingBox::IntersectRay(const Ray &ray, Vector3 &out) const
{
    const float epsilon = 1e-6f;

    Vector3 hit_min(FLT_MIN);
    Vector3 hit_max(FLT_MAX);

    if (fabs(ray.direction.x) < epsilon) {
        if (ray.position.x < m_min.x || ray.position.x > m_max.x) {
            out = Vector3::Zero();
            return false; // invalid. Doesnt collide
        }
    } else {
        hit_min.x = (m_min.x - ray.position.x) / ray.direction.x;
        hit_max.x = (m_max.x - ray.position.x) / ray.direction.x;
        if (hit_min.x > hit_max.x) {
            float temp = hit_min.x;
            hit_min.x = hit_max.x;
            hit_max.x = temp;
        }
    }

    if (fabs(ray.direction.y) < epsilon) {
        if (ray.position.y < m_min.y ||
            ray.position.y > m_max.y) {
            out = Vector3::Zero();
            return false; // invalid. Doesnt collide
        }
    } else {
        hit_min.y = (m_min.y - ray.position.y) / ray.direction.y;
        hit_max.y = (m_max.y - ray.position.y) / ray.direction.y;

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
        if (ray.position.z < m_min.z || ray.position.z > m_max.z) {
            out = Vector3::Zero();
            return false; // invalid. Doesnt collide
        }
    } else {
        hit_min.z = (m_min.z - ray.position.z) / ray.direction.z;
        hit_max.z = (m_max.z - ray.position.z) / ray.direction.z;

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
    if (vec.x < m_min.x) return false;
    if (vec.y < m_min.y) return false;
    if (vec.z < m_min.z) return false;
    if (vec.x > m_max.x) return false;
    if (vec.y > m_max.y) return false;
    if (vec.z > m_max.z) return false;
    return true;
}

double BoundingBox::Area() const
{
    Vector3 dimensions(m_max - m_min);
    return dimensions.x * dimensions.y * dimensions.z;
}
}