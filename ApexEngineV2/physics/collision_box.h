#ifndef COLLISION_BOX_H
#define COLLISION_BOX_H

#include "collision_shape.h"
#include "../math/vector3.h"

namespace apex {
class CollisionBox : public CollisionShape {
public:
    CollisionBox(RigidBody *body, const Vector3 &dimensions);
    CollisionBox(const CollisionBox &other);

    inline const Vector3 &GetDimensions() const { return m_dimensions; }
    inline void SetDimensions(const Vector3 &dimensions) { m_dimensions = dimensions; }

    double TransformToAxis(const Vector3 &axis) const;

protected:
    Vector3 m_dimensions;
};
} // namespace apex

#endif