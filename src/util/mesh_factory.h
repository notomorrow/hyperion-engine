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
class Spatial;
//using RenderableMesh_t = std::tuple<std::shared_ptr<Mesh>, Transform, Material>;

class MeshFactory {
public:
    static std::shared_ptr<Mesh> CreateQuad(
        bool triangle_fan = true);

    static std::shared_ptr<Mesh> CreateCube(
        Vector3 offset = Vector3(0.0));

    static std::shared_ptr<Mesh> CreateCube(
        const BoundingBox &aabb);

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
        const Spatial &a,
        const Spatial &b);

    // merge all meshes into one single mesh
    static std::shared_ptr<Mesh> MergeMeshes(
        const std::vector<Spatial> &meshes);

    // merge meshes, but keep separate by material.
    static std::vector<Spatial> MergeMeshesOnMaterial(
        const std::vector<Spatial> &meshes);

    // apply a transform directly to all vertices of a mesh
    static std::shared_ptr<Mesh> TransformMesh(
        const std::shared_ptr<Mesh> &mesh,
        const Transform &transform);

    // iterate over all child nodes of an entity, collecting meshes,
    // as well as transforms and materials.
    static std::vector<Spatial> GatherMeshes(Node *);

    static std::vector<Spatial> SplitMesh(
        const std::shared_ptr<Mesh> &mesh,
        size_t num_splits);

    struct Voxel {
        BoundingBox aabb;
        bool filled;

        Voxel()
            : aabb(BoundingBox()), filled(false) {}
        Voxel(const BoundingBox &aabb, bool filled = false)
            : aabb(aabb), filled(filled) {}
        Voxel(const Voxel &other)
            : aabb(other.aabb), filled(other.filled) {}
    };

    struct VoxelGrid {
        std::vector<Voxel> voxels;
        size_t size_x, size_y, size_z;
        float voxel_size;
    };

    static VoxelGrid BuildVoxels(const std::shared_ptr<Mesh> &mesh, float voxel_size = 0.1f);
    static std::shared_ptr<Mesh> DebugVoxelMesh(const VoxelGrid &grid);
};
} // namespace hyperion

#endif
