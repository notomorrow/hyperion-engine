#ifndef APEX_PHYSICS_PHYSICS_SHAPE_H
#define APEX_PHYSICS_PHYSICS_SHAPE_H

#include "collision_info.h"
#include "collision_list.h"
#include "../math/vector3.h"
#include "../math/matrix4.h"

namespace apex {
namespace physics {
// forward declarations
class BoxPhysicsShape;
class SpherePhysicsShape;
class PlanePhysicsShape;

enum PhysicsShapeType {
    PhysicsShape_box,
    PhysicsShape_sphere,
    PhysicsShape_plane,
};

// abstract physics shape
class PhysicsShape {
    friend class Rigidbody;
public:
    PhysicsShape(PhysicsShapeType type) : m_type(type) {}
    virtual ~PhysicsShape() = default;

    inline const Matrix4 &GetTransform() const { return m_transform; }
    inline Matrix4 &GetTransform() { return m_transform; }
    inline void SetTransform(const Matrix4 &transform) { m_transform = transform; }
    inline PhysicsShapeType GetType() const { return m_type; }

    Vector3 GetAxis(unsigned int index) const;

    virtual bool CollidesWith(BoxPhysicsShape *shape, CollisionList &out) = 0;
    virtual bool CollidesWith(SpherePhysicsShape *shape, CollisionList &out) = 0;
    virtual bool CollidesWith(PlanePhysicsShape *shape, CollisionList &out) = 0;

protected:
    Matrix4 m_transform;

private:
    const PhysicsShapeType m_type;
};
} // namespace physics
} // namespace apex

#endif