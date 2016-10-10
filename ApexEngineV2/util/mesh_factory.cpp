#include "mesh_factory.h"

namespace apex {

std::shared_ptr<Mesh> MeshFactory::CreateQuad()
{
    auto mesh = std::make_shared<Mesh>();

    std::vector<Vertex> vertices = {
        Vertex(
            Vector3(-1, -1, 0),
            Vector2(0, 0),
            Vector3(0, 0, 1)
            ),
        Vertex(
            Vector3(1, -1, 0),
            Vector2(1, 0),
            Vector3(0, 0, 1)
            ),
        Vertex(
            Vector3(1, 1, 0),
            Vector2(1, 1),
            Vector3(0, 0, 1)
            ),
        Vertex(
            Vector3(-1, 1, 0),
            Vector2(0, 1),
            Vector3(0, 0, 1)
            )
    };

    mesh->SetAttribute(Mesh::ATTR_TEXCOORDS0, Mesh::MeshAttribute::TexCoords0);
    mesh->SetAttribute(Mesh::ATTR_NORMALS, Mesh::MeshAttribute::Normals);

    mesh->SetVertices(vertices);
    mesh->SetPrimitiveType(Mesh::PrimitiveType::PRIM_TRIANGLE_FAN);
    return mesh;
}

} // namespace apex