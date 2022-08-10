#ifndef HYPERION_V2_MESH_BUILDER_H
#define HYPERION_V2_MESH_BUILDER_H

#include <rendering/Mesh.hpp>
#include <core/lib/DynArray.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::Topology;

struct Quad {
    Vertex vertices[4];

    constexpr Vertex &operator[](UInt index) { return vertices[index]; }
    constexpr const Vertex &operator[](UInt index) const { return vertices[index]; }
};

class MeshBuilder {
public:
    static std::unique_ptr<Mesh> Quad(Topology topology = Topology::TRIANGLES);
    static std::unique_ptr<Mesh> Cube();
    static std::unique_ptr<Mesh> NormalizedCubeSphere(UInt num_divisions);

    static std::unique_ptr<Mesh> ApplyTransform(const Mesh *mesh, const Transform &transform);
    static std::unique_ptr<Mesh> Merge(const Mesh *a, const Mesh *b, const Transform &a_transform, const Transform &b_transform);
    static std::unique_ptr<Mesh> Merge(const Mesh *a, const Mesh *b);
};

} // namespace hyperion::v2

#endif
