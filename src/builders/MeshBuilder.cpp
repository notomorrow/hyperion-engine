#include "MeshBuilder.hpp"

namespace hyperion::v2 {

std::unique_ptr<Mesh> MeshBuilder::Quad(Topology topology)
{
    static const std::vector<Vertex> vertices {
        Vertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
    };

    static const std::vector<Mesh::Index> indices {
        0, 3, 2,
        0, 2, 1
    };

    std::unique_ptr<Mesh> mesh;

    const VertexAttributeSet vertex_attributes = renderer::static_mesh_vertex_attributes;

#ifndef HYP_APPLE
    switch (topology) {
    case Topology::TRIANGLE_FAN: {
        auto [new_vertices, new_indices] = Mesh::CalculateIndices(vertices);

        mesh = std::make_unique<Mesh>(
            new_vertices,
            new_indices,
            topology,
            vertex_attributes
        );

        break;
    }
    default:
#endif
        mesh = std::make_unique<Mesh>(
            vertices,
            indices,
            topology,
            vertex_attributes
        );

#ifndef HYP_APPLE
        break;
    }
#endif

    mesh->CalculateTangents();

    return std::move(mesh);
}

std::unique_ptr<Mesh> MeshBuilder::Cube()
{
    static const std::vector<Vertex> vertices {
        Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        Vertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        
        Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        Vertex{{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

        Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        
        Vertex{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

        Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        Vertex{{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        
        Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        Vertex{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},

        Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex{{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        
        Vertex{{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex{{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},

        Vertex{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        Vertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        
        Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        Vertex{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},

        Vertex{{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        
        Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        Vertex{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        Vertex{{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}}
    };

    std::unique_ptr<Mesh> mesh;

    const VertexAttributeSet vertex_attributes = renderer::static_mesh_vertex_attributes;

    auto mesh_data = Mesh::CalculateIndices(vertices);

    mesh = std::make_unique<Mesh>(
        mesh_data.first,
        mesh_data.second,
        Topology::TRIANGLES,
        vertex_attributes
    );

    mesh->CalculateTangents();

    return std::move(mesh);
}

// https://github.com/caosdoar/spheres/blob/master/src/spheres.cpp
std::unique_ptr<Mesh> MeshBuilder::NormalizedCube(UInt num_divisions)
{
    const Float step = 1.0f / static_cast<Float>(num_divisions);

    static const Vector3 origins[6] = {
        Vector3(-1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, 1.0f, -1.0f),
        Vector3(-1.0f, -1.0f, 1.0f)
    };

    static const Vector3 rights[6] = {
        Vector3(2.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, 2.0f),
        Vector3(-2.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, -2.0f),
        Vector3(2.0f, 0.0f, 0.0f),
        Vector3(2.0f, 0.0f, 0.0f)
    };

    static const Vector3 ups[6] = {
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 0.0f, 2.0f),
        Vector3(0.0f, 0.0f, -2.0f)
    };

    std::vector<Vertex> vertices;
    std::vector<Mesh::Index> indices;

    for (UInt face = 0; face < 6; face++) {
        const Vector3 &origin = origins[face];
        const Vector3 &right = rights[face];
        const Vector3 &up = ups[face];

        for (UInt j = 0; j < num_divisions + 1; j++) {
            for (UInt i = 0; i < num_divisions + 1; i++) {
                const Vector3 p = (origin + Vector3(step) * (Vector3(i) * right + Vector3(j) * up)).Normalized();

                const Vector2 uv(
                    static_cast<Float>(i) / static_cast<Float>(num_divisions),
                    static_cast<Float>(j) / static_cast<Float>(num_divisions)
                );

                vertices.push_back(Vertex(p, uv, p));
            }
        }
    }

    const UInt k = num_divisions + 1;

    for (UInt face = 0; face < 6; face++) {
        for (UInt j = 0; j < num_divisions; j++) {
            const bool is_bottom = j < (num_divisions / 2);

            for (UInt i = 0; i < num_divisions; i++) {
                const bool is_left = i < (num_divisions / 2);

                const UInt a = (face * k + j) * k + i;
                const UInt b = (face * k + j) * k + i + 1;
                const UInt c = (face * k + j + 1) * k + i;
                const UInt d = (face * k + j + 1) * k + i + 1;

                if (is_bottom ^ is_left) {
                    indices.push_back(a);
                    indices.push_back(c);
                    indices.push_back(b);
                    indices.push_back(c);
                    indices.push_back(d);
                    indices.push_back(b);
                } else {
                    indices.push_back(a);
                    indices.push_back(c);
                    indices.push_back(d);
                    indices.push_back(a);
                    indices.push_back(d);
                    indices.push_back(b);
                }
            }
        }
    }

    return std::make_unique<Mesh>(
        vertices,
        indices,
        Topology::TRIANGLES,
        renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
    );
}

} // namespace hyperion::v2
