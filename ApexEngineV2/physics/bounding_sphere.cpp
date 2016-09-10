#include "bounding_sphere.h"

namespace apex {
BoundingSphere::BoundingSphere(const Vector3 &center, double radius)
    : m_center(center), m_radius(radius)
{
}

BoundingSphere::BoundingSphere(const BoundingSphere &one, const BoundingSphere &two)
{
    Vector3 center_difference = two.m_center - one.m_center;
    double radius_difference = two.m_radius - one.m_radius;
    double distance = center_difference.LengthSquared();

    if (radius_difference * radius_difference >= distance) {
        if (one.m_radius > two.m_radius) {
            this->m_center = one.m_center;
            this->m_radius = one.m_radius;
        } else {
            this->m_center = two.m_center;
            this->m_radius = two.m_radius;
        }
    } else {
        // partially overlapping.
        distance = sqrt(distance);
        this->m_radius = (distance + one.m_radius + two.m_radius) * 0.5;
        this->m_center = one.m_center;
        
        if (distance > 0) {
            this->m_center += center_difference * ((this->m_radius - one.m_radius) / distance);
        }
    }
}

Vector3 BoundingSphere::GetCenter() const
{
    return m_center;
}

void BoundingSphere::SetCenter(const Vector3 &center)
{
    m_center = center;
}

double BoundingSphere::GetRadius() const
{
    return m_radius;
}

void BoundingSphere::SetRadius(double radius)
{
    m_radius = radius;
}

double BoundingSphere::GetVolume() const
{
    return (4.0 / 3.0) * MathUtil::PI * std::pow(m_radius, 3.0);
}

bool BoundingSphere::Overlaps(const BoundingSphere &other) const
{
    double distance_squared = (m_center - other.m_center).LengthSquared();
    if (distance_squared < std::pow(m_radius + other.m_radius, 2.0)) {
        return true;
    }
    return false;
}
} // namespace apex