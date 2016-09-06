#ifndef AABB_PHYSICS_OBJECT_H
#define AABB_PHYSICS_OBJECT_H

#include "../physics_object.h"
#include "../../math/bounding_box.h"

namespace apex {
class AABBPhysicsObject final : public PhysicsObject {
public:
    AABBPhysicsObject(const std::string &tag, 
        double mass, double restitution, const BoundingBox &aabb);

    bool RayTest(const Ray &ray, Vector3 &out) const;

    bool CheckCollision(AABBPhysicsObject *other, Vector3 &contactnormal, float &distance);
    bool CheckCollision(MeshPhysicsObject *other, Vector3 &contactnormal, float &distance);

protected:
    BoundingBox aabb;
};
}

#endif