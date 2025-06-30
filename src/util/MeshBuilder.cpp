/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <util/MeshBuilder.hpp>

#include <core/math/Triangle.hpp>

#include <scene/Mesh.hpp>

namespace hyperion {

const Array<Vertex> MeshBuilder::quadVertices = {
    Vertex { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }
};

const Array<uint32> MeshBuilder::quadIndices = {
    0, 3, 2,
    0, 2, 1
};

const Array<Vertex> MeshBuilder::cubeVertices = {
    Vertex { { -1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
    Vertex { { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
    Vertex { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },

    Vertex { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
    Vertex { { -1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
    Vertex { { -1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },

    Vertex { { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
    Vertex { { -1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
    Vertex { { -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },

    Vertex { { -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
    Vertex { { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
    Vertex { { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },

    Vertex { { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
    Vertex { { 1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
    Vertex { { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },

    Vertex { { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
    Vertex { { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
    Vertex { { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },

    Vertex { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { 1.0f, 1.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },

    Vertex { { 1.0f, 1.0f, -1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { 1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
    Vertex { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },

    Vertex { { 1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
    Vertex { { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
    Vertex { { -1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },

    Vertex { { -1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
    Vertex { { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
    Vertex { { 1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },

    Vertex { { -1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
    Vertex { { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
    Vertex { { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },

    Vertex { { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
    Vertex { { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
    Vertex { { -1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } }
};

Handle<Mesh> MeshBuilder::Quad()
{
    const VertexAttributeSet vertexAttributes = staticMeshVertexAttributes;

    Handle<Mesh> mesh = CreateObject<Mesh>(
        quadVertices,
        quadIndices,
        TOP_TRIANGLES,
        vertexAttributes);

    mesh->SetName(NAME("MeshBuilder_Quad"));

    mesh->CalculateTangents();

    return mesh;
}

Handle<Mesh> MeshBuilder::Cube()
{
    Handle<Mesh> mesh;

    const VertexAttributeSet vertexAttributes = staticMeshVertexAttributes;

    auto meshData = Mesh::CalculateIndices(cubeVertices);

    mesh = CreateObject<Mesh>(
        meshData.first,
        meshData.second,
        TOP_TRIANGLES,
        vertexAttributes);
    mesh->SetName(NAME("MeshBuilder_Cube"));

    mesh->CalculateTangents();

    return mesh;
}

Handle<Mesh> MeshBuilder::NormalizedCubeSphere(uint32 numDivisions)
{
    const float step = 1.0f / float(numDivisions);

    static const Vec3f origins[6] = {
        Vector3(-1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, 1.0f, -1.0f),
        Vector3(-1.0f, -1.0f, 1.0f)
    };

    static const Vec3f rights[6] = {
        Vector3(2.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, 2.0f),
        Vector3(-2.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, -2.0f),
        Vector3(2.0f, 0.0f, 0.0f),
        Vector3(2.0f, 0.0f, 0.0f)
    };

    static const Vec3f ups[6] = {
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 0.0f, 2.0f),
        Vector3(0.0f, 0.0f, -2.0f)
    };

    Array<Vertex> vertices;
    Array<Mesh::Index> indices;

    for (uint32 face = 0; face < 6; face++)
    {
        const Vec3f& origin = origins[face];
        const Vec3f& right = rights[face];
        const Vec3f& up = ups[face];

        for (uint32 j = 0; j < numDivisions + 1; j++)
        {
            for (uint32 i = 0; i < numDivisions + 1; i++)
            {
                const Vec3f point = (origin + Vec3f(step) * (Vec3f(i) * right + Vec3f(j) * up)).Normalized();
                Vec3f position = point;
                Vec3f normal = point;

                const Vec2f uv(
                    float(j + (face * numDivisions)) / float(numDivisions * 6),
                    float(i + (face * numDivisions)) / float(numDivisions * 6));

                vertices.PushBack(Vertex(position, uv));
            }
        }
    }

    const uint32 k = numDivisions + 1;

    for (uint32 face = 0; face < 6; face++)
    {
        for (uint32 j = 0; j < numDivisions; j++)
        {
            const bool isBottom = j < (numDivisions / 2);

            for (uint32 i = 0; i < numDivisions; i++)
            {
                const bool isLeft = i < (numDivisions / 2);

                const uint32 a = (face * k + j) * k + i;
                const uint32 b = (face * k + j) * k + i + 1;
                const uint32 c = (face * k + j + 1) * k + i;
                const uint32 d = (face * k + j + 1) * k + i + 1;

                if (isBottom ^ isLeft)
                {
                    indices.PushBack(a);
                    indices.PushBack(c);
                    indices.PushBack(b);
                    indices.PushBack(c);
                    indices.PushBack(d);
                    indices.PushBack(b);
                }
                else
                {
                    indices.PushBack(a);
                    indices.PushBack(c);
                    indices.PushBack(d);
                    indices.PushBack(a);
                    indices.PushBack(d);
                    indices.PushBack(b);
                }
            }
        }
    }

    Handle<Mesh> mesh = CreateObject<Mesh>(
        vertices,
        indices,
        TOP_TRIANGLES,
        staticMeshVertexAttributes);

    mesh->SetName(NAME("MeshBuilder_NormalizedCubeSphere"));

    mesh->CalculateNormals(true);
    mesh->CalculateTangents();

    return mesh;
}

Handle<Mesh> MeshBuilder::ApplyTransform(const Mesh* mesh, const Transform& transform)
{
    AssertThrow(mesh != nullptr);

    StreamedMeshData* streamedMeshData = mesh->GetStreamedMeshData();

    if (!streamedMeshData)
    {
        return Handle<Mesh> {};
    }

    ResourceHandle resourceHandle(*streamedMeshData);

    const Matrix4 normalMatrix = transform.GetMatrix().Inverted().Transposed();

    Array<Vertex> newVertices = streamedMeshData->GetMeshData().vertices;

    for (Vertex& vertex : newVertices)
    {
        vertex.SetPosition(transform.GetMatrix() * vertex.GetPosition());
        vertex.SetNormal(normalMatrix * vertex.GetNormal());
        vertex.SetTangent(normalMatrix * vertex.GetTangent());
        vertex.SetBitangent(normalMatrix * vertex.GetBitangent());
    }

    Handle<Mesh> newMesh = CreateObject<Mesh>(
        std::move(newVertices),
        streamedMeshData->GetMeshData().indices,
        mesh->GetTopology(),
        mesh->GetVertexAttributes());
    newMesh->SetName(mesh->GetName());

    return newMesh;
}

Handle<Mesh> MeshBuilder::Merge(const Mesh* a, const Mesh* b, const Transform& aTransform, const Transform& bTransform)
{
    AssertThrow(a != nullptr);
    AssertThrow(b != nullptr);

    Handle<Mesh> transformedMeshes[] = {
        ApplyTransform(a, aTransform),
        ApplyTransform(b, bTransform)
    };

    StreamedMeshData* streamedMeshDatas[] = {
        transformedMeshes[0]->GetStreamedMeshData(),
        transformedMeshes[1]->GetStreamedMeshData()
    };

    AssertThrow(streamedMeshDatas[0] != nullptr);
    AssertThrow(streamedMeshDatas[1] != nullptr);

    TResourceHandle<StreamedMeshData> streamedMeshDataRefs[] = {
        *streamedMeshDatas[0],
        *streamedMeshDatas[1]
    };

    const VertexAttributeSet mergedVertexAttributes = a->GetVertexAttributes() | b->GetVertexAttributes();

    Array<Vertex> allVertices;
    allVertices.Resize(streamedMeshDataRefs[0]->GetMeshData().vertices.Size() + streamedMeshDataRefs[1]->GetMeshData().vertices.Size());

    Array<Mesh::Index> allIndices;
    allIndices.Resize(streamedMeshDataRefs[0]->GetMeshData().indices.Size() + streamedMeshDataRefs[1]->GetMeshData().indices.Size());

    SizeType vertexOffset = 0,
             indexOffset = 0;

    for (SizeType meshIndex = 0; meshIndex < 2; meshIndex++)
    {
        const SizeType vertexOffsetBefore = vertexOffset;

        for (SizeType i = 0; i < streamedMeshDataRefs[meshIndex]->GetMeshData().vertices.Size(); i++)
        {
            allVertices[vertexOffset++] = streamedMeshDataRefs[meshIndex]->GetMeshData().vertices[i];
        }

        for (SizeType i = 0; i < streamedMeshDataRefs[meshIndex]->GetMeshData().indices.Size(); i++)
        {
            allIndices[indexOffset++] = streamedMeshDataRefs[meshIndex]->GetMeshData().indices[i] + vertexOffsetBefore;
        }
    }

    Handle<Mesh> newMesh = CreateObject<Mesh>(
        std::move(allVertices),
        std::move(allIndices),
        a->GetTopology(),
        mergedVertexAttributes);

    newMesh->SetName(NAME("MeshBuilder_MergedMesh"));

    return newMesh;
}

Handle<Mesh> MeshBuilder::Merge(const Mesh* a, const Mesh* b)
{
    return Merge(a, b, Transform(), Transform());
}

Handle<Mesh> MeshBuilder::BuildVoxelMesh(VoxelGrid voxelGrid)
{
    Handle<Mesh> mesh;

    for (uint32 x = 0; x < voxelGrid.size.x; x++)
    {
        for (uint32 y = 0; y < voxelGrid.size.y; y++)
        {
            for (uint32 z = 0; z < voxelGrid.size.z; z++)
            {
                uint32 index = voxelGrid.GetIndex(x, y, z);

                if (!voxelGrid.voxels[index].filled)
                {
                    continue;
                }

                auto cube = Cube();

                if (!mesh)
                {
                    mesh = ApplyTransform(
                        cube.Get(),
                        Transform(Vec3f { float(x), float(y), float(z) } * voxelGrid.voxelSize, voxelGrid.voxelSize, Quaternion::Identity()));
                }
                else
                {
                    mesh = Merge(
                        mesh.Get(),
                        cube.Get(),
                        Transform(),
                        Transform(Vec3f { float(x), float(y), float(z) } * voxelGrid.voxelSize, voxelGrid.voxelSize, Quaternion::Identity()));
                }
            }
        }
    }

    if (!mesh)
    {
        mesh = Cube();
    }

    return mesh;
}

} // namespace hyperion
