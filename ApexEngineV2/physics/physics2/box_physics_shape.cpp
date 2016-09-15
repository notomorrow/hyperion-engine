#include "box_physics_shape.h"
#include "box_collision.h"
#include "sphere_physics_shape.h"
#include <cassert>

namespace apex {
namespace physics {
BoxPhysicsShape::BoxPhysicsShape(const Vector3 &dimensions)
    : PhysicsShape(PhysicsShape_box), m_dimensions(dimensions)
{
}

bool BoxPhysicsShape::CollidesWith(BoxPhysicsShape *other, CollisionInfo &out)
{
    Vector3 to_center = other->GetAxis(3) - GetAxis(3);

    double penetration = std::numeric_limits<double>::max();
    unsigned int best = std::numeric_limits<unsigned int>::max();

    for (int i = 0; i < 3; i++) {
        if (!BoxCollision::TryAxis(*this, *other, GetAxis(i), to_center,
            i, penetration, best)) {
            return false;
        }
    }

    for (int i = 0; i < 3; i++) {
        if (!BoxCollision::TryAxis(*this, *other, other->GetAxis(i), to_center,
            i + 3, penetration, best)) {
            return false;
        }
    }

    unsigned int best_single_axis = best;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            Vector3 axis = GetAxis(i);
            axis.Cross(other->GetAxis(j));

            unsigned int index = (i * 3 + j) + 6;

            if (!BoxCollision::TryAxis(*this, *other, axis, to_center,
                index, penetration, best)) {
                return false;
            }
        }
    }

    // check to make sure there was a result
    assert(best != std::numeric_limits<decltype(best)>::max());

    if (best < 3) {
        BoxCollision::FillPointFaceBoxBox(*this, *other, to_center, out, best, penetration);
        return true;
    } else if (best < 6) {
        BoxCollision::FillPointFaceBoxBox(*other, *this, to_center * -1.0f, out, best - 3, penetration);
        return true;
    } else {
        best -= 6;
        unsigned int a_axis_index = best / 3;
        unsigned int b_axis_index = best % 3;

        Vector3 a_axis = GetAxis(a_axis_index);
        Vector3 b_axis = other->GetAxis(b_axis_index);

        Vector3 axis = a_axis;
        axis.Cross(b_axis);
        axis.Normalize();

        if (axis.Dot(to_center) > 0) {
            axis *= -1.0f;
        }

        Vector3 a_point_on_edge = m_dimensions * 0.5f;
        Vector3 b_point_on_edge = other->m_dimensions * 0.5f;

        for (unsigned int i = 0; i < 3; i++) {
            if (i == a_axis_index) {
                a_point_on_edge[i] = 0;
            } else if (GetAxis(i).Dot(axis) > 0) {
                a_point_on_edge[i] *= -1;
            }

            if (i == b_axis_index) {
                b_point_on_edge[i] = 0;
            } else if (other->GetAxis(i).Dot(axis) < 0) {
                b_point_on_edge[i] *= -1;
            }
        }

        a_point_on_edge *= m_transform;
        b_point_on_edge *= other->m_transform;

        Vector3 vertex = BoxCollision::ContactPoint(a_point_on_edge, a_axis, m_dimensions[a_axis_index] * 0.5f,
            b_point_on_edge, b_axis, other->m_dimensions[b_axis_index] * 0.5f,
            best_single_axis > 2);

        out.m_contact_point = vertex;
        out.m_contact_normal = axis;
        out.m_contact_penetration = penetration;
        return true;
    }

    return false;
}

bool BoxPhysicsShape::CollidesWith(SpherePhysicsShape *sphere, CollisionInfo &out)
{
    Matrix4 transform = m_transform;
    Matrix4 inverse = transform;
    inverse.Invert();

    Vector3 center = sphere->GetAxis(3);
    Vector3 center_transformed = center * inverse;

    if (fabs(center_transformed.GetX()) - sphere->GetRadius() > (m_dimensions.GetX() * 0.5f) ||
        fabs(center_transformed.GetY()) - sphere->GetRadius() > (m_dimensions.GetY() * 0.5f) ||
        fabs(center_transformed.GetZ()) - sphere->GetRadius() > (m_dimensions.GetZ()* 0.5f)) {
        return false;
    }

    Vector3 closest = Vector3::Zero();

    double distance = center_transformed.GetX();
    if (distance > m_dimensions.GetX() * 0.5f) {
        distance = m_dimensions.GetX() * 0.5f;
    }
    if (distance < -(m_dimensions.GetX() * 0.5f)) {
        distance = -(m_dimensions.GetX() * 0.5f);
    }
    closest.SetX(distance);

    distance = center_transformed.GetY();
    if (distance > m_dimensions.GetY() * 0.5f) {
        distance = m_dimensions.GetY() * 0.5f;
    }
    if (distance < -(m_dimensions.GetY() * 0.5f)) {
        distance = -(m_dimensions.GetY() * 0.5f);
    }
    closest.SetY(distance);

    distance = center_transformed.GetZ();
    if (distance > m_dimensions.GetZ() * 0.5f) {
        distance = m_dimensions.GetZ() * 0.5f;
    }
    if (distance < -(m_dimensions.GetZ()* 0.5f)) {
        distance = -(m_dimensions.GetZ()* 0.5f);
    }
    closest.SetZ(distance);

    distance = (closest - center_transformed).LengthSquared();
    if (distance > sphere->GetRadius() * sphere->GetRadius()) {
        return false;
    }

    Vector3 closest_transformed = closest * m_transform;

    Vector3 contact_normal = closest_transformed - center;
    contact_normal.Normalize();


    out.m_contact_point = closest_transformed;
    out.m_contact_normal = contact_normal;
    out.m_contact_penetration = sphere->GetRadius() - sqrt(distance);
    return true;
}
} // namespace physics
} // namespace apex