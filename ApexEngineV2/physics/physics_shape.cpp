#include "physics_shape.h"

#include "../util.h"

namespace apex {
namespace physics {

PhysicsShape::PhysicsShape(PhysicsShapeType type)
    : m_type(type),
      m_collision_shape(nullptr)
{
}

PhysicsShape::~PhysicsShape()
{
}

Vector3 PhysicsShape::GetAxis(unsigned int index) const
{
    ex_assert(index < 8);

    return Vector3(m_transform.values[index],
        m_transform.values[index + 4],
        m_transform.values[index + 8]);
}

} // namespace physics
} // namespace apex
