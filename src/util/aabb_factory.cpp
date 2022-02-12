#include "aabb_factory.h"

namespace hyperion {

BoundingBox AABBFactory::CreateMeshBoundingBox(const std::shared_ptr<Mesh> &mesh)
{
    BoundingBox aabb;
    auto vertices = mesh->GetVertices();
    for (auto &&vert : vertices) {
        aabb.Extend(vert.GetPosition());
    }
    return aabb;
}

BoundingBox AABBFactory::CreateEntityBoundingBox(const std::shared_ptr<Entity> &entity)
{
    BoundingBox aabb;
    if (entity->GetRenderable() != nullptr) {
        std::shared_ptr<Mesh> mesh = nullptr;
        if (mesh = std::dynamic_pointer_cast<Mesh>(entity->GetRenderable())) {
            aabb.Extend(CreateMeshBoundingBox(mesh));
        }
    }

    for (size_t i = 0; i < entity->NumChildren(); i++) {
        if (auto child = entity->GetChild(i)) {
            aabb.Extend(CreateEntityBoundingBox(child));
        }
    }

    /*Transform local_transform;
    local_transform.SetTranslation(entity->GetLocalTranslation());
    local_transform.SetRotation(entity->GetLocalRotation());
    local_transform.SetScale(entity->GetLocalScale());
    aabb *= local_transform.GetMatrix();*/

    return aabb;
}

} // namespace hyperion
