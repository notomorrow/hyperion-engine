/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <util/MeshBuilder.hpp>

#include <core/math/Triangle.hpp>

#include <rendering/Mesh.hpp>

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

    MeshData meshData;
    meshData.desc.numIndices = uint32(quadIndices.Size());
    meshData.desc.numVertices = uint32(quadVertices.Size());
    meshData.vertexData = quadVertices;
    meshData.indexData.SetSize(quadIndices.Size() * sizeof(uint32));
    meshData.indexData.Write(quadIndices.Size() * sizeof(uint32), 0, quadIndices.Data());

    meshData.CalculateTangents();

    Handle<Mesh> mesh = CreateObject<Mesh>();
    mesh->SetMeshData(meshData);
    mesh->SetName(NAME("MeshBuilder_Quad"));

    return mesh;
}

Handle<Mesh> MeshBuilder::Cube()
{
    static const auto cubeVerticesAndIndices = Mesh::CalculateIndices(cubeVertices);

    MeshData meshData;
    meshData.desc.numIndices = uint32(cubeVerticesAndIndices.second.Size());
    meshData.desc.numVertices = uint32(cubeVerticesAndIndices.first.Size());
    meshData.vertexData = cubeVerticesAndIndices.first;
    meshData.indexData.SetSize(cubeVerticesAndIndices.second.Size() * sizeof(uint32));
    meshData.indexData.Write(cubeVerticesAndIndices.second.Size() * sizeof(uint32), 0, cubeVerticesAndIndices.second.Data());

    meshData.CalculateTangents();

    Handle<Mesh> mesh = CreateObject<Mesh>();
    mesh->SetName(NAME("MeshBuilder_Cube"));
    mesh->SetMeshData(meshData);

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
    Array<uint32> indices;

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

    MeshData meshData;
    meshData.desc.numIndices = uint32(indices.Size());
    meshData.desc.numVertices = uint32(vertices.Size());
    meshData.vertexData = std::move(vertices);
    meshData.indexData.SetSize(indices.Size() * sizeof(uint32));
    meshData.indexData.Write(indices.Size() * sizeof(uint32), 0, indices.Data());

    meshData.CalculateNormals(true);
    meshData.CalculateTangents();

    Handle<Mesh> mesh = CreateObject<Mesh>();
    mesh->SetMeshData(meshData);
    mesh->SetName(NAME("MeshBuilder_NormalizedCubeSphere"));

    return mesh;
}

Handle<Mesh> MeshBuilder::ApplyTransform(const Mesh* mesh, const Transform& transform)
{
    Assert(mesh != nullptr);

    if (!mesh->GetAsset())
    {
        return Handle<Mesh> {};
    }

    ResourceHandle resourceHandle(*mesh->GetAsset()->GetResource());

    const Matrix4 normalMatrix = transform.GetMatrix().Inverted().Transposed();

    MeshData meshData = *mesh->GetAsset()->GetMeshData();

    for (Vertex& vertex : meshData.vertexData)
    {
        vertex.SetPosition(transform.GetMatrix() * vertex.GetPosition());
        vertex.SetNormal(normalMatrix * vertex.GetNormal());
        vertex.SetTangent(normalMatrix * vertex.GetTangent());
        vertex.SetBitangent(normalMatrix * vertex.GetBitangent());
    }

    Handle<Mesh> newMesh = CreateObject<Mesh>();
    newMesh->SetMeshData(meshData);
    newMesh->SetName(mesh->GetName());

    return newMesh;
}

Handle<Mesh> MeshBuilder::Merge(const Mesh* a, const Mesh* b, const Transform& aTransform, const Transform& bTransform)
{
    Assert(a != nullptr && a->GetAsset().IsValid());
    Assert(b != nullptr && b->GetAsset().IsValid());

    Handle<Mesh> transformedMeshes[] = {
        ApplyTransform(a, aTransform),
        ApplyTransform(b, bTransform)
    };

    ResourceHandle resourceHandles[] = {
        ResourceHandle(*transformedMeshes[0]->GetAsset()->GetResource()),
        ResourceHandle(*transformedMeshes[1]->GetAsset()->GetResource())
    };

    MeshData* meshDatas[] = {
        transformedMeshes[0]->GetAsset()->GetMeshData(),
        transformedMeshes[1]->GetAsset()->GetMeshData()
    };

    Assert(meshDatas[0] != nullptr);
    Assert(meshDatas[1] != nullptr);

    const VertexAttributeSet mergedVertexAttributes = a->GetVertexAttributes() | b->GetVertexAttributes();

    Array<Vertex> allVertices;
    allVertices.Resize(meshDatas[0]->vertexData.Size() + meshDatas[1]->vertexData.Size());

    Array<uint32> allIndices;
    allIndices.Resize(
        (meshDatas[0]->indexData.Size() / GpuElemTypeSize(meshDatas[0]->desc.meshAttributes.indexBufferElemType))
        + (meshDatas[1]->indexData.Size() / GpuElemTypeSize(meshDatas[1]->desc.meshAttributes.indexBufferElemType)));

    SizeType vertexOffset = 0;
    SizeType indexOffset = 0;

    for (SizeType meshIndex = 0; meshIndex < 2; meshIndex++)
    {
        const SizeType vertexOffsetBefore = vertexOffset;

        for (SizeType i = 0; i < meshDatas[meshIndex]->vertexData.Size(); i++)
        {
            allVertices[vertexOffset++] = meshDatas[meshIndex]->vertexData[i];
        }

        const uint32 stride = GpuElemTypeSize(meshDatas[meshIndex]->desc.meshAttributes.indexBufferElemType);

        for (SizeType i = 0; i < meshDatas[meshIndex]->indexData.Size() / stride; i++)
        {
            allIndices[indexOffset++] = meshDatas[meshIndex]->indexData[i * stride] + vertexOffsetBefore;
        }
    }

    MeshData mergedMeshData;
    mergedMeshData.desc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    mergedMeshData.desc.meshAttributes.vertexAttributes = mergedVertexAttributes;
    mergedMeshData.desc.numIndices = uint32(allIndices.Size());
    mergedMeshData.desc.numVertices = uint32(allVertices.Size());
    mergedMeshData.vertexData = std::move(allVertices);
    mergedMeshData.indexData.SetSize(allIndices.Size() * sizeof(uint32));
    mergedMeshData.indexData.Write(allIndices.Size() * sizeof(uint32), 0, allIndices.Data());

    Handle<Mesh> newMesh = CreateObject<Mesh>();
    newMesh->SetMeshData(mergedMeshData);
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
