#include "mesh_factory.h"
#include "../math/math_util.h"
#include "../entity.h"

namespace hyperion {

std::shared_ptr<Mesh> MeshFactory::CreateQuad(bool triangle_fan)
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

    const std::vector<MeshIndex> quad_indices = {
        0, 3, 2,
        0, 2, 1
    };

    mesh->SetAttribute(Mesh::ATTR_TEXCOORDS0, Mesh::MeshAttribute::TexCoords0);
    mesh->SetAttribute(Mesh::ATTR_NORMALS, Mesh::MeshAttribute::Normals);

    if (triangle_fan) {
        mesh->SetVertices(vertices);
        mesh->SetPrimitiveType(Mesh::PrimitiveType::PRIM_TRIANGLE_FAN);
    } else {
        mesh->SetVertices(vertices, quad_indices);
        mesh->SetPrimitiveType(Mesh::PrimitiveType::PRIM_TRIANGLES);
    }

    return mesh;
}

std::shared_ptr<Mesh> MeshFactory::TransformMesh(const std::shared_ptr<Mesh> &mesh,
    const Transform &transform)
{
    auto new_mesh = std::make_shared<Mesh>();

    std::map<Mesh::MeshAttributeType, Mesh::MeshAttribute> all_mesh_attributes;

    all_mesh_attributes.insert(mesh->GetAttributes().begin(), mesh->GetAttributes().end());

    std::vector<Vertex> all_vertices = mesh->GetVertices();
    std::vector<MeshIndex> all_indices = mesh->GetIndices();

    for (Vertex &a_vertex : all_vertices) {
        a_vertex.SetPosition(a_vertex.GetPosition() * transform.GetMatrix());
        // a_vertex.SetNormal(a_vertex.GetNormal() * transform.GetMatrix());
        // a_vertex.SetTangent(a_vertex.GetTangent() * transform.GetMatrix());
        // a_vertex.SetBitangent(a_vertex.GetBitangent() * transform.GetMatrix());
    }

    for (auto it : all_mesh_attributes) {
        new_mesh->SetAttribute(it.first, it.second);
    }

    new_mesh->SetVertices(all_vertices, all_indices);
    new_mesh->SetPrimitiveType(mesh->GetPrimitiveType());

    return new_mesh;
}

std::shared_ptr<Mesh> MeshFactory::MergeMeshes(const std::shared_ptr<Mesh> &a,
    const std::shared_ptr<Mesh> &b,
    Transform transform_a,
    Transform transform_b)
{
    // TODO: raise error if primitive types differ
    std::shared_ptr<Mesh> new_mesh = std::make_shared<Mesh>(),
        a_transformed = TransformMesh(a, transform_a),
        b_transformed = TransformMesh(b, transform_b);

    std::map<Mesh::MeshAttributeType, Mesh::MeshAttribute> all_mesh_attributes;

    all_mesh_attributes.insert(a_transformed->GetAttributes().begin(), a_transformed->GetAttributes().end());
    all_mesh_attributes.insert(b_transformed->GetAttributes().begin(), b_transformed->GetAttributes().end());

    std::vector<Vertex> all_vertices;
    all_vertices.reserve(a_transformed->GetVertices().size() + b_transformed->GetVertices().size());

    std::vector<MeshIndex> all_indices;
    all_indices.reserve(a_transformed->GetIndices().size() + b_transformed->GetIndices().size());

    for (Vertex a_vertex : a_transformed->GetVertices()) {
        all_vertices.push_back(a_vertex);
    }

    for (MeshIndex a_index : a_transformed->GetIndices()) {
        all_indices.push_back(a_index);
    }

    const size_t b_index_offset = all_vertices.size();

    for (Vertex b_vertex : b_transformed->GetVertices()) {
        all_vertices.push_back(b_vertex);
    }

    for (MeshIndex b_index : b_transformed->GetIndices()) {
        all_indices.push_back(b_index_offset + b_index);
    }

    for (auto it : all_mesh_attributes) {
        new_mesh->SetAttribute(it.first, it.second);
    }

    new_mesh->SetVertices(all_vertices, all_indices);
    new_mesh->SetPrimitiveType(Mesh::PrimitiveType::PRIM_TRIANGLES);

    return new_mesh;
}

std::shared_ptr<Mesh> MeshFactory::MergeMeshes(const RenderableMesh_t &a, const RenderableMesh_t &b)
{
    return MergeMeshes(std::get<0>(a), std::get<0>(b), std::get<1>(a), std::get<1>(b));
}

std::shared_ptr<Mesh> MeshFactory::MergeMeshes(const std::vector<RenderableMesh_t> &meshes)
{
    std::shared_ptr<Mesh> mesh;

    for (auto &it : meshes) {
        if (mesh == nullptr) {
            mesh = std::make_shared<Mesh>();
        }

        mesh = MergeMeshes(std::make_tuple(mesh, Transform(), Material()), it);
    }

    return mesh;
}

std::shared_ptr<Mesh> MeshFactory::CreateCube(Vector3 offset)
{
    const std::vector<Transform> sides = {
        Transform(Vector3(0, 0, -1), Vector3::One(), Quaternion::Identity()),
        Transform(Vector3(0, 0, 1), Vector3::One(), Quaternion(Vector3::UnitY(), MathUtil::DegToRad(180.0f))),
        Transform(Vector3(1, 0, 0), Vector3::One(), Quaternion(Vector3::UnitY(), MathUtil::DegToRad(90.0f))),
        Transform(Vector3(-1, 0, 0), Vector3::One(), Quaternion(Vector3::UnitY() * -1, MathUtil::DegToRad(90.0f))),
        Transform(Vector3(0, 1, 0), Vector3::One(), Quaternion(Vector3::UnitX() * -1, MathUtil::DegToRad(90.0f))),
        Transform(Vector3(0, -1, 0), Vector3::One(), Quaternion(Vector3::UnitX(), MathUtil::DegToRad(90.0f)))
        // Transform(Vector3::Zero(), Vector3::One(), Quaternion(Vector3::UnitX() * 1, MathUtil::DegToRad(180.0f)) * Quaternion(Vector3::UnitZ(), MathUtil::DegToRad(180.0f))),
        // Transform(Vector3(-1, 0, -1), Vector3::One(), Quaternion(Vector3::UnitY() * -1, MathUtil::DegToRad(90.0f))),
        // Transform(Vector3(1, 0, -1), Vector3::One(), Quaternion(Vector3::UnitY() * -1, MathUtil::DegToRad(-90.0f))),
        // Transform(Vector3(0, 0, -2), Vector3::One(), Quaternion(Vector3::UnitX() * -1, MathUtil::DegToRad(0.0f))),
        // Transform(Vector3(0, -1, -1), Vector3::One(), Quaternion(Vector3::UnitX() * -1, MathUtil::DegToRad(-90.0f))),
        // Transform(Vector3(0, 1, -1), Vector3::One(), Quaternion(Vector3::UnitX() * -1, MathUtil::DegToRad(90.0f)))
    };

    std::shared_ptr<Mesh> mesh;

    for (auto transform : sides) {
        if (mesh == nullptr) {
            mesh = std::make_shared<Mesh>();
        }

        mesh = MergeMeshes(
            mesh,
            CreateQuad(false),
            Transform(),
            transform
        );
    }

    // position it so that position is defined as the center of the cube.

    mesh = TransformMesh(
        mesh,
        Transform(
            offset,
            Vector3::One(),
            Quaternion::Identity()
        )
    );

    mesh->CalculateNormals();

    return mesh;
}

std::vector<RenderableMesh_t> MeshFactory::GatherMeshes(Entity *entity)
{
    ex_assert(entity != nullptr);

    entity->UpdateTransform();

    std::vector<RenderableMesh_t> meshes;
    meshes.reserve(10);

    if (auto mesh = std::dynamic_pointer_cast<Mesh>(entity->GetRenderable())) {
        meshes.push_back(std::make_tuple(
            mesh,
            entity->GetGlobalTransform(),
            entity->GetMaterial()
        ));
    }

    for (size_t i = 0; i < entity->NumChildren(); i++) {
        std::vector<RenderableMesh_t> sub_meshes = GatherMeshes(entity->GetChild(i).get());

        meshes.insert(meshes.end(), sub_meshes.begin(), sub_meshes.end());
    }

    return meshes;
}

} // namespace hyperion
