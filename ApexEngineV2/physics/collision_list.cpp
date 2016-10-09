#include "collision_list.h"

namespace apex {
namespace physics {
CollisionList::CollisionList()
{
    // reserve space for a few collision objects, to save runtime performance.
    m_collisions.reserve(4);
}

CollisionList::CollisionList(const CollisionList &other)
    : m_collisions(other.m_collisions)
{
}

} // namespace physics
} // namespace apex