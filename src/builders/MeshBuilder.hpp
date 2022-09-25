#ifndef HYPERION_V2_MESH_BUILDER_H
#define HYPERION_V2_MESH_BUILDER_H

#include <rendering/Mesh.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/UniquePtr.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::Topology;

struct Quad
{
    Vertex vertices[4];

    constexpr Vertex &operator[](UInt index) { return vertices[index]; }
    constexpr const Vertex &operator[](UInt index) const { return vertices[index]; }
};

class MeshBuilder
{
    static const std::vector<Vertex> quad_vertices;
    static const std::vector<Mesh::Index> quad_indices;
    static const std::vector<Vertex> cube_vertices;

public:
    static UniquePtr<Mesh> Quad(Topology topology = Topology::TRIANGLES);
    static UniquePtr<Mesh> Cube();
    static UniquePtr<Mesh> NormalizedCubeSphere(UInt num_divisions);

    static UniquePtr<Mesh> ApplyTransform(const Mesh *mesh, const Transform &transform);
    static UniquePtr<Mesh> Merge(const Mesh *a, const Mesh *b, const Transform &a_transform, const Transform &b_transform);
    static UniquePtr<Mesh> Merge(const Mesh *a, const Mesh *b);
};

} // namespace hyperion::v2

#endif
