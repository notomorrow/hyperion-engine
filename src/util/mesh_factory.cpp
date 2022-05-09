#include "mesh_factory.h"
#include "../math/math_util.h"
#include "../scene/node.h"
#include "../scene/spatial.h"
#include "../hash_code.h"

#include <unordered_map>

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

    mesh->EnableAttribute(Mesh::ATTR_TEXCOORDS0);
    mesh->EnableAttribute(Mesh::ATTR_NORMALS);

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
    if (mesh == nullptr) {
        return nullptr;
    }

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
        new_mesh->EnableAttribute(it.first);
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
        new_mesh->EnableAttribute(it.first);
    }

    new_mesh->SetVertices(all_vertices, all_indices);
    new_mesh->SetPrimitiveType(Mesh::PrimitiveType::PRIM_TRIANGLES);

    new_mesh->SetShader(b->GetShader()); // hmm..

    return new_mesh;
}

std::shared_ptr<Mesh> MeshFactory::MergeMeshes(const Spatial &a, const Spatial &b)
{
    return MergeMeshes(
        std::dynamic_pointer_cast<Mesh>(a.GetRenderable()),
        std::dynamic_pointer_cast<Mesh>(b.GetRenderable()),
        a.GetTransform(),
        b.GetTransform()
    );
}

std::shared_ptr<Mesh> MeshFactory::MergeMeshes(const std::vector<Spatial> &meshes)
{
    std::shared_ptr<Mesh> mesh;

    for (auto &it : meshes) {
        if (mesh == nullptr) {
            mesh = std::make_shared<Mesh>();
        }

        mesh = MergeMeshes(
            Spatial(Spatial::Bucket::RB_OPAQUE, mesh, Material(), BoundingBox(), Transform()),
            it
        );
    }

    return mesh;
}

std::vector<Spatial> MeshFactory::MergeMeshesOnMaterial(const std::vector<Spatial> &meshes)
{
    std::unordered_map<HashCode_t, Spatial> renderable_map;

    for (auto &renderable : meshes) {
        Material material = renderable.GetMaterial();
        HashCode_t material_hash_code = material.GetHashCode().Value();

        auto it = renderable_map.find(material_hash_code);

        if (it == renderable_map.end()) {
            renderable_map[material_hash_code] = Spatial(
                Spatial::Bucket::RB_OPAQUE,
                std::make_shared<Mesh>(),
                material,
                BoundingBox(),
                Transform()
            );
        }

        auto merged_mesh = MergeMeshes(
            renderable_map[material_hash_code],
            renderable
        );

        renderable_map[material_hash_code] = Spatial(
            Spatial::Bucket::RB_OPAQUE,
            merged_mesh,
            material,
            BoundingBox(),
            Transform()
        );
    }

    std::vector<Spatial> values;
    values.reserve(renderable_map.size());

    for (auto &it : renderable_map) {
        values.push_back(it.second);
    }

    return values;
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

std::shared_ptr<Mesh> MeshFactory::CreateCube(const BoundingBox &aabb)
{
    auto mesh = std::make_shared<Mesh>();

#define MESH_FACTORY_QUAD_INDICES(a, b, c, d) a, b, c, a, c, d
    const std::vector<MeshIndex> indices = {
        0, 1, 1, 2, 2, 3,
        3, 0, 0, 4, 4, 5,
        5, 3, 5, 6, 6, 7,
        4, 7, 7, 1, 6, 2
        /*MESH_FACTORY_QUAD_INDICES(1, 0, 3, 2),
        MESH_FACTORY_QUAD_INDICES(2, 3, 7, 6),
        MESH_FACTORY_QUAD_INDICES(3, 0, 4, 7),
        MESH_FACTORY_QUAD_INDICES(6, 5, 1, 2),
        MESH_FACTORY_QUAD_INDICES(4, 5, 6, 7),
        MESH_FACTORY_QUAD_INDICES(5, 4, 0, 1)*/
    };
#undef MESH_FACTORY_QUAD_INDICES

    std::vector<Vertex> vertices;
    vertices.resize(8);

    const auto &corners = aabb.GetCorners();

    for (int i = 0; i < corners.size(); i++) {
        vertices[i].SetPosition(corners[i]);
    }

    mesh->SetVertices(vertices, indices);
    mesh->CalculateNormals();

    return mesh;
}

// https://www.danielsieger.com/blog/2021/03/27/generating-spheres.html
std::shared_ptr<Mesh> MeshFactory::CreateSphere(float radius, int num_slices, int num_stacks)
{
    std::vector<Vertex> vertices;
    std::vector<MeshIndex> indices;

    // top vertex
    vertices.push_back(Vertex(Vector3(0, 1, 0)));
    indices.push_back(0);
    MeshIndex v0 = indices.back(); 

    for (int i = 0; i < num_stacks - 1; i++) {
        auto phi = MathUtil::pi<double> * double(i + 1) / double(num_stacks);

        for (int j = 0; j < num_slices; j++) {
            double theta = 2.0 * MathUtil::pi<double> * double(j) / double(num_slices);
            double x = std::sin(phi) * std::cos(theta);
            double y = std::cos(phi);
            double z = std::sin(phi) * std::sin(theta);

            vertices.push_back(Vertex(Vector3(x, y, z)));
            // indices.push_back(indices.size());
        }
    }

    // bottom vertex
    vertices.push_back(Vertex(Vector3(0, -1, 0)));
    indices.push_back(indices.size());
    MeshIndex v1 = indices.back();

    // add top / bottom triangles
    for (int i = 0; i < num_slices; ++i) {
        int i0 = i + 1;
        int i1 = (i + 1) % num_slices + 1;

        indices.push_back(v0);
        indices.push_back(i1);
        indices.push_back(i0);

        i0 = i + num_slices * (num_stacks - 2) + 1;
        i1 = (i + 1) % num_slices + num_slices * (num_stacks - 2) + 1;

        indices.push_back(v1);
        indices.push_back(i0);
        indices.push_back(i1);
    }

    // add quads per stack / slice
    for (int j = 0; j < num_stacks - 2; j++)
    {
        auto j0 = j * num_slices + 1;
        auto j1 = (j + 1) * num_slices + 1;
        for (int i = 0; i < num_slices; i++) {
            auto i0 = j0 + i;
            auto i1 = j0 + (i + 1) % num_slices;
            auto i2 = j1 + (i + 1) % num_slices;
            auto i3 = j1 + i;

            indices.push_back(i0);
            indices.push_back(i3);
            indices.push_back(i2);
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);
        }
    }

    std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
    mesh->SetVertices(vertices, indices);

    return mesh;
}

std::vector<Spatial> MeshFactory::GatherMeshes(Node *node)
{
    AssertThrow(node != nullptr);

    node->UpdateTransform();

    std::vector<Spatial> meshes;
    meshes.reserve(10);

    if (auto mesh = std::dynamic_pointer_cast<Mesh>(node->GetRenderable())) {
        meshes.push_back(node->GetSpatial());
    }

    for (size_t i = 0; i < node->NumChildren(); i++) {
        if (auto *child = node->GetChild(i).get()) {
            std::vector<Spatial> sub_meshes = GatherMeshes(child);

            meshes.insert(meshes.end(), sub_meshes.begin(), sub_meshes.end());
        }
    }

    return meshes;
}

std::vector<Spatial> MeshFactory::SplitMesh(
    const std::shared_ptr<Mesh> &mesh,
    size_t num_splits)
{
    auto triangles = mesh->CalculateTriangleBuffer();
    std::sort(triangles.begin(), triangles.end());

    std::vector<BoundingBox> aabbs;
    std::vector<std::vector<Triangle>> sub_triangles; // map aabbs to tris

    BoundingBox total_aabb = mesh->GetAABB();

    sub_triangles.resize(num_splits * num_splits * num_splits);
    aabbs.resize(num_splits * num_splits * num_splits);
    
    // setup aabbs
    for (int x = 0; x < num_splits; x++) {
        for (int y = 0; y < num_splits; y++) {
            for (int z = 0; z < num_splits; z++) {
                size_t index = (x * num_splits * num_splits) + (y * num_splits) + z;
                aabbs[index] = total_aabb;
                aabbs[index].SetMax(total_aabb.GetMax() * (1.0f / num_splits));
                aabbs[index].SetCenter(aabbs[index].GetCenter() + (aabbs[index].GetDimensions() * Vector3(x, y, z)));
            }
        }
    }

    // place triangles into correct subarrays
    for (size_t i = 0; i < triangles.size(); i++) {
        for (size_t j = 0; j < aabbs.size(); j++) {
            if (aabbs[j].ContainsPoint(triangles[i][0].GetPosition())
                || aabbs[j].ContainsPoint(triangles[i][1].GetPosition())
                || aabbs[j].ContainsPoint(triangles[i][2].GetPosition())) {
                sub_triangles[j].push_back(triangles[i]);
            }
        }
    }

    std::vector<Spatial> meshes;

    for (size_t i = 0; i < sub_triangles.size(); i++) {
        auto new_mesh = std::make_shared<Mesh>();
        new_mesh->SetShader(mesh->GetShader());
        new_mesh->SetTriangles(sub_triangles[i]);
        meshes.push_back(Spatial(
            Spatial::Bucket::RB_OPAQUE,
            new_mesh,
            Material(),
            BoundingBox(),
            Transform()
        ));
    }

    return meshes;
}

MeshFactory::VoxelGrid MeshFactory::BuildVoxels(const std::shared_ptr<Mesh> &mesh, float voxel_size)
{
    BoundingBox total_aabb = mesh->GetAABB();
    Vector3 total_aabb_dimensions = total_aabb.GetDimensions();

    size_t num_voxels_x = MathUtil::Ceil(total_aabb_dimensions.x / voxel_size);
    size_t num_voxels_y = MathUtil::Ceil(total_aabb_dimensions.y / voxel_size);
    size_t num_voxels_z = MathUtil::Ceil(total_aabb_dimensions.z / voxel_size);


    // building out grid
    VoxelGrid grid;
    grid.voxel_size = voxel_size;
    grid.size_x = num_voxels_x;
    grid.size_y = num_voxels_y;
    grid.size_z = num_voxels_z;

    if (!num_voxels_x || !num_voxels_y || !num_voxels_z) {
        return grid;
    }

    grid.voxels.resize(num_voxels_x * num_voxels_y * num_voxels_z);

    for (size_t x = 0; x < num_voxels_x; x++) {
        for (size_t y = 0; y < num_voxels_y; y++) {
            for (size_t z = 0; z < num_voxels_z; z++) {
                size_t index = (z * num_voxels_x * num_voxels_y) + (y * num_voxels_y) + x;
                grid.voxels[index] = Voxel(
                    BoundingBox(
                        Vector3(float(x), float(y), float(z)) * voxel_size,
                        Vector3(float(x + 1), float(y + 1), float(z + 1)) * voxel_size
                    ),
                    false
                );
            }
        }
    }

    // filling in data
    for (auto &index : mesh->GetIndices()) {
        const auto &vertex = mesh->GetVertices()[index];
        Vector3 vertex_over_dimensions = (vertex.GetPosition() - total_aabb.GetMin()) / Vector3::Max(total_aabb.GetDimensions(), 0.0001f);

        size_t x = MathUtil::Floor(MathUtil::Clamp(vertex_over_dimensions.x * (num_voxels_x - 1), 0.0f, num_voxels_x - 1.0f));
        size_t y = MathUtil::Floor(MathUtil::Clamp(vertex_over_dimensions.y * (num_voxels_y - 1), 0.0f, num_voxels_y - 1.0f));
        size_t z = MathUtil::Floor(MathUtil::Clamp(vertex_over_dimensions.z * (num_voxels_z - 1), 0.0f, num_voxels_z - 1.0f));

        size_t voxel_index = (z * num_voxels_x * num_voxels_y) + (y * num_voxels_y) + x;

        grid.voxels[voxel_index].filled = true;
    }

    return grid;
}

std::shared_ptr<Mesh> MeshFactory::DebugVoxelMesh(const VoxelGrid &grid)
{
    auto mesh = std::make_shared<Mesh>();

    for (size_t x = 0; x < grid.size_x; x++) {
        for (size_t y = 0; y < grid.size_y; y++) {
            for (size_t z = 0; z < grid.size_z; z++) {
                size_t index = (z * grid.size_x * grid.size_y) + (y * grid.size_y) + x;

                if (!grid.voxels[index].filled) {
                    continue;
                }

                mesh = MergeMeshes(
                    mesh,
                    CreateCube(),
                    Transform(),
                    Transform(Vector3(float(x), float(y), float(z)) * grid.voxel_size, grid.voxel_size, Quaternion::Identity())
                );
            }
        }
    }

    return mesh;
}

} // namespace hyperion
