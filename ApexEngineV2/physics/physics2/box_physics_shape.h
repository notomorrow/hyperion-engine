#ifndef APEX_PHYSICS_BOX_PHYSICS_SHAPE_H
#define APEX_PHYSICS_BOX_PHYSICS_SHAPE_H

#include "physics_shape.h"
#include "../../math/vector3.h"

namespace apex {
namespace physics {
class BoxPhysicsShape : public PhysicsShape {
public:
    BoxPhysicsShape(const Vector3 &dimensions);
    BoxPhysicsShape(const BoxPhysicsShape &other);

    inline const Vector3 &GetDimensions() const { return m_dimensions; }
    inline Vector3 &GetDimensions() { return m_dimensions; }
    inline void SetDimensions(const Vector3 &dimensions) { m_dimensions = dimensions; }

    bool CollidesWith(BoxPhysicsShape *shape, CollisionList &out);
    bool CollidesWith(SpherePhysicsShape *shape, CollisionList &out);
    bool CollidesWith(PlanePhysicsShape *shape, CollisionList &out);

private:
    Vector3 m_dimensions;
};
} // namespace physics
} // namespace apex

#endif