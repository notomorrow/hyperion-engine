#ifndef MESH_FACTORY_H
#define MESH_FACTORY_H

#include "../rendering/mesh.h"
#include "../math/transform.h"

#include <memory>

namespace apex {
class MeshFactory {
public:
    static std::shared_ptr<Mesh> CreateQuad(bool triangle_fan = true);
    static std::shared_ptr<Mesh> CreateCube();
    static std::shared_ptr<Mesh> MergeMeshes(const std::shared_ptr<Mesh> &a,
        const std::shared_ptr<Mesh> &b,
        Transform transform_a,
        Transform transform_b);
    static std::shared_ptr<Mesh> TransformMesh(const std::shared_ptr<Mesh> &mesh,
        const Transform &transform);
};
} // namespace apex

#endif
