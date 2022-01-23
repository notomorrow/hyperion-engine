#ifndef PHYSICS_OBJECT_H
#define PHYSICS_OBJECT_H

#include "../math/matrix4.h"
#include "../math/transform.h"
#include "../math/ray.h"

#include <functional>
#include <string>

#define MAX_VELOCITY 20

namespace hyperion {
// forward declarations
class AABBPhysicsObject;
class MeshPhysicsObject;

enum PhysicsShape {
    AABB_physics_shape,
};

class PhysicsObject {
    friend class PhysicsControl;
    friend class PhysicsManager;
public:
    PhysicsObject(const std::string &tag, 
        double mass, double restitution, PhysicsShape shape);
    virtual ~PhysicsObject();

    const Vector3 &GetForce() const;
    void SetForce(const Vector3 &vec);
    const Vector3 &GetAcceleration() const;
    void SetAcceleration(const Vector3 &vec);
    const Vector3 &GetVelocity() const;
    void SetVelocity(const Vector3 &vec);
    const Vector3 &GetGravity() const;
    void SetGravity(const Vector3 &vec);

    void ApplyForce(const Vector3 &vec);

    virtual bool RayTest(const Ray &ray, Vector3 &out) const = 0;

    virtual bool CheckCollision(AABBPhysicsObject *other, Vector3 &contactnormal, float &distance) = 0;
    virtual bool CheckCollision(MeshPhysicsObject *other, Vector3 &contactnormal, float &distance) = 0;

protected:
    PhysicsShape shape;
    Transform transform;
    std::string tag;
    double dynamic_friction = 0.5;
    double static_friction = 0.5;
    double mass;
    double restitution;
    bool grounded = false;
    Vector3 ground_diff;
    Vector3 position;
    Vector3 force;
    Vector3 acceleration;
    Vector3 last_acceleration;
    Vector3 velocity;
    Vector3 gravity;
};
} // namespace hyperion

#endif
