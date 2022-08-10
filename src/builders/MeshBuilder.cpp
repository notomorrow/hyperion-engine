#include "MeshBuilder.hpp"

// temp
#include <util/NoiseFactory.hpp>

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
std::unique_ptr<Mesh> MeshBuilder::NormalizedCubeSphere(UInt num_divisions)
{
    // // temp
    // auto *noise_generator = NoiseFactory::GetInstance()->Capture(NoiseGenerationType::WORLEY_NOISE, 12345);
    // auto *simplex = NoiseFactory::GetInstance()->Capture(NoiseGenerationType::SIMPLEX_NOISE, 12345);

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
                const Vector3 point = (origin + Vector3(step) * (Vector3(i) * right + Vector3(j) * up)).Normalized();
                Vector3 position = point;
                const Vector3 normal = point;

                const Vector2 uv(
                    static_cast<Float>(j + (face * num_divisions)) / static_cast<Float>(num_divisions * 6),
                    static_cast<Float>(i + (face * num_divisions)) / static_cast<Float>(num_divisions * 6)
                );

                // const Float noise_value = noise_generator->GetNoise(point.x * 3.0f, point.y * 3.0f, point.z * 3.0f);
                // const Float simplex_value = simplex->GetNoise(point.x * 10.25f, point.y * 10.25f, point.z * 10.25f);
                    
                //     //(j + face * (num_divisions + 1)) * 1000.0, (i + face * (num_divisions + 1)) * 1000.0);
                // position += (normal * ((simplex_value) * 2.0f + ((noise_value))));


                vertices.push_back(Vertex(position, uv, normal));
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

    // NoiseFactory::GetInstance()->Release(noise_generator);
    // NoiseFactory::GetInstance()->Release(simplex);

    return std::make_unique<Mesh>(
        vertices,
        indices,
        Topology::TRIANGLES,
        renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
    );
}

std::unique_ptr<Mesh> MeshBuilder::ApplyTransform(const Mesh *mesh, const Transform &transform)
{
    AssertThrow(mesh != nullptr);

    std::vector<Vertex> new_vertices(mesh->GetVertices());

    const auto normal_matrix = transform.GetMatrix().Inverted().Transposed();

    for (Vertex &vertex : new_vertices) {
        vertex.SetPosition(transform.GetMatrix() * vertex.GetPosition());
        vertex.SetNormal(normal_matrix * vertex.GetNormal());
        vertex.SetTangent(normal_matrix * vertex.GetTangent());
        vertex.SetBitangent(normal_matrix * vertex.GetBitangent());
    }

    return std::make_unique<Mesh>(
        new_vertices,
        mesh->GetIndices(),
        mesh->GetTopology(),
        mesh->GetVertexAttributes(),
        mesh->GetFlags()
    );
}

std::unique_ptr<Mesh> MeshBuilder::Merge(const Mesh *a, const Mesh *b, const Transform &a_transform, const Transform &b_transform)
{
    AssertThrow(a != nullptr);
    AssertThrow(b != nullptr);

    std::unique_ptr<Mesh> transformed_meshes[2] = { ApplyTransform(a, a_transform), ApplyTransform(b, b_transform) };
    
    const auto merged_vertex_attributes = a->GetVertexAttributes() | b->GetVertexAttributes();

    std::vector<Vertex> all_vertices(transformed_meshes[0]->GetVertices().size() + transformed_meshes[1]->GetVertices().size());
    std::vector<Mesh::Index> all_indices(transformed_meshes[0]->GetIndices().size() + transformed_meshes[1]->GetIndices().size());

    SizeType vertex_offset = 0,
             index_offset = 0;

    for (SizeType mesh_index = 0; mesh_index < 2; mesh_index++) {
        for (SizeType i = 0; i < transformed_meshes[mesh_index]->GetVertices().size(); i++) {
            all_vertices[vertex_offset++] = transformed_meshes[mesh_index]->GetVertices()[i];
        }

        for (SizeType i = 0; i < transformed_meshes[mesh_index]->GetIndices().size(); i++) {
            all_indices[index_offset++] = transformed_meshes[mesh_index]->GetIndices()[i];
        }
    }

    return std::make_unique<Mesh>(
        all_vertices,
        all_indices,
        a->GetTopology(),
        merged_vertex_attributes,
        a->GetFlags()
    );
}

std::unique_ptr<Mesh> MeshBuilder::Merge(const Mesh *a, const Mesh *b)
{
    return Merge(a, b, Transform(), Transform());
}

} // namespace hyperion::v2
