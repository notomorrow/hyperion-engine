#include "collision_box.h"

namespace apex {
double CollisionBox::TransformToAxis(const Vector3 &axis) const
{
    return m_half_size.GetX() * abs(axis.Dot(GetAxis(0))) +
        m_half_size.GetY() * abs(axis.Dot(GetAxis(1))) +
        m_half_size.GetZ() * abs(axis.Dot(GetAxis(2)));
}
} // namespace apex