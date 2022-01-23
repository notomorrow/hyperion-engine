#include "box_collision.h"

namespace hyperion {
namespace physics {

double BoxCollision::TransformToAxis(const BoxPhysicsShape &box, const Vector3 &axis)
{
    return (box.GetDimensions().GetX() * 0.5f) * fabs(axis.Dot(box.GetAxis(0))) +
        (box.GetDimensions().GetY() * 0.5f) * fabs(axis.Dot(box.GetAxis(1))) +
        (box.GetDimensions().GetZ() * 0.5f) * fabs(axis.Dot(box.GetAxis(2)));
}

void BoxCollision::FillPointFaceBoxBox(const BoxPhysicsShape &a, const BoxPhysicsShape &b,
    const Vector3 &to_center, CollisionInfo &out,
    unsigned int best, double penetration)
{
    Vector3 normal = a.GetAxis(best);
    if (a.GetAxis(best).Dot(to_center) > 0.0f) {
        normal *= -1.0f;
    }

    Vector3 vertex = b.GetDimensions() * 0.5f;
    for (int i = 0; i < 3; i++) {
        if (b.GetAxis(i).Dot(normal) < 0.0f) {
            vertex[i] *= -1.0f;
        }
    }

    out.m_contact_point = vertex * b.GetTransform();
    out.m_contact_normal = normal;
    out.m_contact_penetration = penetration;
}

Vector3 BoxCollision::ContactPoint(const Vector3 &a_point, const Vector3 &a_dir, double a_size,
    const Vector3 &b_point, const Vector3 &b_dir, double b_size,
    bool outside_edge)
{
    double a_len_sqr = double(a_dir.LengthSquared());
    double b_len_sqr = double(b_dir.LengthSquared());
    double b_dot_a = b_dir.Dot(a_dir);

    Vector3 dist = a_point - b_point;
    double a_dist = a_dir.Dot(dist);
    double b_dist = b_dir.Dot(dist);

    double denom = (a_len_sqr * b_len_sqr) - (b_dot_a * b_dot_a);

    if (fabs(denom) < MathUtil::EPSILON) {
        return outside_edge ? a_point : b_point;
    }

    double mua = (b_dot_a * b_dist - b_len_sqr * a_dist) / denom;
    double mub = (a_len_sqr * b_dist - b_dot_a * a_dist) / denom;

    if (mua > a_size || mua < -a_size || mub > b_size || mub < -b_size) {
        return outside_edge ? a_point : b_point;
    } else {
        return (a_point + (a_dir * mua)) * 0.5 +
            (b_point + (b_dir * mub)) * 0.5;
    }
}

double BoxCollision::PenetrationOnAxis(const BoxPhysicsShape &a, const BoxPhysicsShape &b,
    const Vector3 &axis, const Vector3 &to_center)
{
    double a_proj = TransformToAxis(a, axis);
    double b_proj = TransformToAxis(b, axis);

    double dist = fabs(to_center.Dot(axis));

    return (a_proj + b_proj) - dist;
}

bool BoxCollision::TryAxis(const BoxPhysicsShape &a, const BoxPhysicsShape &b,
    Vector3 axis, const Vector3 &to_center, unsigned int index,
    double &out_smallest_penetration, unsigned int &out_smallest_case)
{
    if (axis.LengthSquared() < MathUtil::EPSILON) {
        return true;
    }

    axis.Normalize();

    double penetration = PenetrationOnAxis(a, b, axis, to_center);
    if (penetration < 0.0) {
        return false;
    }

    if (penetration < out_smallest_penetration) {
        out_smallest_penetration = penetration;
        out_smallest_case = index;
    }

    return true;
}

} // namespace physics
} // namespace hyperion
