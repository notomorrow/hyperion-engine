#ifndef COLLISION_SHAPE_H
#define COLLISION_SHAPE_H

#include "rigid_body.h"
#include "../math/matrix4.h"

namespace apex {
class CollisionShape {
public:
    CollisionShape(RigidBody *body);
    CollisionShape(RigidBody *body, const Matrix4 &transform);
    CollisionShape(RigidBody *body, const Matrix4 &transform, const Matrix4 &offset);

    inline RigidBody *GetBody() const { return m_body; }
    inline const Matrix4 &GetTransform() const { return m_transform; }

    Vector3 GetAxis(unsigned int index) const;

    void UpdateTransform();

protected:
    RigidBody *m_body;
    Matrix4 m_offset;
    Matrix4 m_transform;
};
} // namespace apex

#endif