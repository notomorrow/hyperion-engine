#include "collision_list.h"

namespace apex {
namespace physics {
CollisionList::CollisionList()
{
    // reserve space for a few collision objects, to save runtime performance.
    m_collisions.reserve(4);
}
} // namespace physics
} // namespace apex