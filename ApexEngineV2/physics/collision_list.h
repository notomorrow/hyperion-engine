#ifndef APEX_PHYSICS_COLLISION_LIST_H
#define APEX_PHYSICS_COLLISION_LIST_H

#include "collision_info.h"

#include <vector>

namespace apex {
namespace physics {
struct CollisionList {
    std::vector<CollisionInfo> m_collisions;

    CollisionList();
    CollisionList(const CollisionList &other);
};

} // namespace physics
} // namespace apex

#endif