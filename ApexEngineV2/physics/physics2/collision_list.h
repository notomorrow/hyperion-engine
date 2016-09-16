#ifndef APEX_PHYSICS_COLLISION_LIST_H
#define APEX_PHYSICS_COLLISION_LIST_H

#include "collision_info.h"

#include <vector>

namespace apex {
namespace physics {
struct CollisionList {
    std::vector<CollisionInfo> m_collisions;

    CollisionList();
};
} // namespace physics
} // namespace apex

#endif