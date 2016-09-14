#include "collision_shape.h"
#include <cassert>

namespace apex {
CollisionShape::CollisionShape(RigidBody *body)
    : m_body(body)
{
}

CollisionShape::CollisionShape(RigidBody *body, const Matrix4 &transform)
    : m_body(body), m_transform(transform)
{
}

CollisionShape::CollisionShape(RigidBody *body, const Matrix4 &transform, const Matrix4 &offset)
    : m_body(body), m_transform(transform), m_offset(offset)
{
}

Vector3 CollisionShape::GetAxis(unsigned int index) const
{
    assert(index < 8);
    return Vector3(m_transform.values[index],
        m_transform.values[index + 4 /*1*/],
        m_transform.values[index + 8 /*2*/]);
}

void CollisionShape::UpdateTransform()
{
    m_transform = m_body->m_transform * m_offset;
}
} // namespace apex