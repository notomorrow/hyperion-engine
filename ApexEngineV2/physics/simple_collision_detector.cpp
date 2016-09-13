#include "simple_collision_detector.h"

namespace apex {
static bool OverlapOnAxis(const CollisionBox &a, const CollisionBox &b,
    const Vector3 &axis, const Vector3 &to_center)
{
    double a_proj = a.TransformToAxis(axis);
    double b_proj = b.TransformToAxis(axis);

    double dist = fabs(to_center.Dot(axis));

    return bool(dist < a_proj + b_proj);
}

bool SimpleCollisionDetector::SphereAndHalfSpace(const CollisionSphere &sphere, const CollisionPlane &plane)
{
    double distance = plane.m_direction.Dot(sphere.GetAxis(3)) - sphere.GetRadius();
    return bool(distance <= plane.m_offset);
}

bool SimpleCollisionDetector::SphereAndSphere(const CollisionSphere &a, const CollisionSphere &b)
{
    Vector3 mid = a.GetAxis(3) - b.GetAxis(3);
    return bool(mid.LengthSquared() <
        (a.GetRadius() + b.GetRadius()) * (a.GetRadius() + b.GetRadius()));
}

bool SimpleCollisionDetector::BoxAndHalfSpace(const CollisionBox &box, const CollisionPlane &plane)
{
    double proj_rad = box.TransformToAxis(plane.m_direction);
    double dist = plane.m_direction.Dot(box.GetAxis(3)) - proj_rad;
    return bool(dist <= plane.m_offset);
}

bool SimpleCollisionDetector::BoxAndBox(const CollisionBox &a, const CollisionBox &b)
{
    Vector3 to_center = b.GetAxis(3) - a.GetAxis(3);

    // test box a's axes
    for (int i = 0; i < 3; i++) {
        if (!OverlapOnAxis(a, b, a.GetAxis(i), to_center)) {
            return false;
        }
    }
    // test box b's axes
    for (int i = 0; i < 3; i++) {
        if (!OverlapOnAxis(a, b, b.GetAxis(i), to_center)) {
            return false;
        }
    }

    // test cross products
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            Vector3 axis = a.GetAxis(i);
            axis.Cross(b.GetAxis(j));
            if (!OverlapOnAxis(a, b, axis, to_center)) {
                return false;
            }
        }
    }

    return true;
}
} // namespace apex