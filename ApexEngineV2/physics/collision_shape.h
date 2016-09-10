#ifndef COLLISION_SHAPE_H
#define COLLISION_SHAPE_H

#include "rigid_body.h"
#include "../math/matrix4.h"

namespace apex {
class CollisionShape {
    friend class SimpleCollisionDetector;
    friend class ComplexCollisionDetector;
public:
    RigidBody *m_body;
    Matrix4 m_offset;

    void CalculateInternals();
    Vector3 GetAxis(unsigned int index) const;
    const Matrix4 &GetTransform() const;

protected:
    Matrix4 m_transform;
};
} // namespace apex

#endif