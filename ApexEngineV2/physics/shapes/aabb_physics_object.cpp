#include "aabb_physics_object.h"
#include <algorithm>

namespace apex {
AABBPhysicsObject::AABBPhysicsObject(const std::string &tag, 
    double mass, double restituion, const BoundingBox &aabb)
    : aabb(aabb), PhysicsObject(tag, mass, restituion, AABB_physics_shape)
{
}

bool AABBPhysicsObject::RayTest(const Ray &ray, Vector3 &out) const
{
    BoundingBox trans(aabb);
    trans *= transform;

    RaytestHit raytest_hit;

    if (bool intersects = trans.IntersectRay(ray, raytest_hit)) {
        out = raytest_hit.hitpoint;

        return true;
    }

    return false;
}

bool AABBPhysicsObject::CheckCollision(AABBPhysicsObject *other, 
    Vector3 &contact_normal, float &distance)
{
    BoundingBox a(aabb);
    a *= transform;

    BoundingBox b(other->aabb);
    b *= other->transform;

    static const Vector3 faces[6] = {
        Vector3(-1, 0, 0),
        Vector3(1, 0, 0),
        Vector3(0, -1, 0),
        Vector3(0, 1, 0),
        Vector3(0, 0, -1),
        Vector3(0, 0, 1),
    };

    const float distances[6] = {
        (b.GetMax().x - a.GetMin().x),
        (a.GetMax().x - b.GetMin().x),
        (b.GetMax().y - a.GetMin().y),
        (a.GetMax().y - b.GetMin().y),
        (b.GetMax().z - a.GetMin().z),
        (a.GetMax().z - b.GetMin().z)
    };

    for (int i = 0; i < 6; i++) {
        if (distances[i] < 0.0) {
            return false;
        }

        if (i == 0 || distances[i] < distance) {
            distance = distances[i];
            contact_normal = faces[i];
        }
    }

    return true;
}

bool AABBPhysicsObject::CheckCollision(MeshPhysicsObject *other, 
    Vector3 &contactnormal, float &distance)
{
    /// \todo
    return false;
}
}
