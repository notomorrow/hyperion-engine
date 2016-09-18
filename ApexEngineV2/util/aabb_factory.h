#ifndef AABB_FACTORY_H
#define AABB_FACTORY_H

#include "../math/bounding_box.h"
#include "../rendering/mesh.h"
#include "../entity.h"

namespace apex {
class AABBFactory {
public:
    static BoundingBox CreateMeshBoundingBox(const std::shared_ptr<Mesh> &mesh);
    static BoundingBox CreateEntityBoundingBox(const std::shared_ptr<Entity> &entity);
};
} // namespace apex

#endif