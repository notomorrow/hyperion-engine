#ifndef MESH_FACTORY_H
#define MESH_FACTORY_H

#include "../rendering/mesh.h"
#include "../math/transform.h"

#include <memory>
#include <utility>

namespace hyperion {
class Entity;
using MeshWithTransform_t = std::pair<std::shared_ptr<Mesh>, Transform>;

class MeshFactory {
public:
    static std::shared_ptr<Mesh> CreateQuad(bool triangle_fan = true);
    static std::shared_ptr<Mesh> CreateCube(Vector3 offset = Vector3(0.0));
    static std::shared_ptr<Mesh> MergeMeshes(
        const std::shared_ptr<Mesh> &a,
        const std::shared_ptr<Mesh> &b,
        Transform transform_a,
        Transform transform_b);
    static std::shared_ptr<Mesh> MergeMeshes(
        const MeshWithTransform_t &a,
        const MeshWithTransform_t &b);
    static std::shared_ptr<Mesh> MergeMeshes(const std::vector<MeshWithTransform_t> &meshes);
    static std::shared_ptr<Mesh> TransformMesh(const std::shared_ptr<Mesh> &mesh,
        const Transform &transform);

    static std::vector<MeshWithTransform_t> GatherMeshes(Entity *entity);
};
} // namespace hyperion

#endif
