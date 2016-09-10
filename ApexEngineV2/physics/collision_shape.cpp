#include "collision_shape.h"
#include <cassert>

namespace apex {
void CollisionShape::CalculateInternals()
{
    m_transform = m_body->transform * m_offset;
}

Vector3 CollisionShape::GetAxis(unsigned int index) const
{
    assert(index < 8);
    return Vector3(m_transform.values[index],
        m_transform.values[index + 4],
        m_transform.values[index + 8]);
}

const Matrix4 &CollisionShape::GetTransform() const
{
    return m_transform;
}
} // namespace apex