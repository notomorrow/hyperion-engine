#include "physics_shape.h"
#include <cassert>

namespace apex {
namespace physics {

Vector3 PhysicsShape::GetAxis(unsigned int index) const
{
    assert(index < 8);
    return Vector3(m_transform.values[index],
        m_transform.values[index + 4],
        m_transform.values[index + 8]);
}

} // namespace physics
} // namespace apex