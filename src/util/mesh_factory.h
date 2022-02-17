#ifndef MESH_FACTORY_H
#define MESH_FACTORY_H

#include "../rendering/mesh.h"
#include "../rendering/material.h"
#include "../math/transform.h"

#include <memory>
#include <utility>
#include <tuple>

namespace hyperion {
class Node;
using RenderableMesh_t = std::tuple<std::shared_ptr<Mesh>, Transform, Material>;

class MeshFactory {
public:
    static std::shared_ptr<Mesh> CreateQuad(
        bool triangle_fan = true);

    static std::shared_ptr<Mesh> CreateCube(
        Vector3 offset = Vector3(0.0));

    static std::shared_ptr<Mesh> CreateSphere(
        float radius,
        int num_slices = 16,
        int num_stacks = 8);

    // merge two meshes into one (perserving transform)
    static std::shared_ptr<Mesh> MergeMeshes(
        const std::shared_ptr<Mesh> &a,
        const std::shared_ptr<Mesh> &b,
        Transform transform_a,
        Transform transform_b);

    // merge two meshes into one (perserving transform)
    static std::shared_ptr<Mesh> MergeMeshes(
        const RenderableMesh_t &a,
        const RenderableMesh_t &b);

    // merge all meshes into one single mesh
    static std::shared_ptr<Mesh> MergeMeshes(
        const std::vector<RenderableMesh_t> &meshes);

    // merge meshes, but keep separate by material.
    static std::vector<RenderableMesh_t> MergeMeshesOnMaterial(
        const std::vector<RenderableMesh_t> &meshes);

    // apply a transform directly to all vertices of a mesh
    static std::shared_ptr<Mesh> TransformMesh(
        const std::shared_ptr<Mesh> &mesh,
        const Transform &transform);

    // iterate over all child nodes of an entity, collecting meshes,
    // as well as transforms and materials.
    static std::vector<RenderableMesh_t> GatherMeshes(Node *);
};
} // namespace hyperion

#endif
