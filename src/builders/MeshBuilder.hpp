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
    // struct Cube {
    //     Cube();

    //     std::unique_ptr<Mesh> Build();
    // };

    // struct NormalizedCube {
    //     NormalizedCube(UInt num_divisions);

    //     std::unique_ptr<Mesh> Build();

    //     UInt num_divisions;
    //     DynArray<struct Quad> quads;
    // };

    static std::unique_ptr<Mesh> Quad(Topology topology = Topology::TRIANGLES);
    static std::unique_ptr<Mesh> Cube();
    static std::unique_ptr<Mesh> NormalizedCube(UInt num_divisions);
};

} // namespace hyperion::v2

#endif
