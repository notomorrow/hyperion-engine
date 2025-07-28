/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <util/MeshBuilder.hpp>

#include <core/math/Triangle.hpp>

#include <rendering/Mesh.hpp>

#include <scene/util/VoxelOctree.hpp>

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

Handle<Mesh> MeshBuilder::BuildVoxelMesh(const VoxelOctree& voxelOctree)
{
    BoundingBox minVoxelAabb;

    Array<Vec3i> voxelPositions;

    Proc<void(const VoxelOctree&)> traverse;
    traverse = [&](const VoxelOctree& octant)
    {
        if (octant.IsDivided())
        {
            AssertDebug(octant.GetEntries().Empty());

            for (auto& childOctant : octant.GetOctants())
            {
                Assert(childOctant.octree != nullptr);

                traverse(static_cast<const VoxelOctree&>(*childOctant.octree));
            }
        }
        else if (octant.GetEntries().Any()) // filled voxel node
        {
            Vec3f center = octant.GetAABB().GetCenter();
            voxelPositions.PushBack(Vec3i((int)center.x, (int)center.y, (int)center.z));

            if (!minVoxelAabb.IsValid())
            {
                minVoxelAabb = octant.GetAABB();

                return;
            }

            minVoxelAabb.SetExtent(MathUtil::Min(minVoxelAabb.GetExtent(), octant.GetAABB().GetExtent()));
        }
    };

    traverse(voxelOctree);

    HashSet<Vec3i> voxelSet;
    voxelSet.Reserve(voxelPositions.Size());

    for (uint32 i = 0; i < voxelPositions.Size(); i++)
    {
        voxelSet.Insert(voxelPositions[i]);
    }

    Array<Vertex> vertices;
    Array<uint32> indices;
    uint32 vertexOffset = 0;

    struct Face
    {
        Vec3f normal; // Face normal direction
        Vec3f uDir;   // First tangent direction
        Vec3f vDir;   // Second tangent direction
    };

    static const Face faces[6] = {
        { Vec3f(1, 0, 0), Vec3f(0, 1, 0), Vec3f(0, 0, 1) },  // +X
        { Vec3f(-1, 0, 0), Vec3f(0, 1, 0), Vec3f(0, 0, 1) }, // -X
        { Vec3f(0, 1, 0), Vec3f(1, 0, 0), Vec3f(0, 0, 1) },  // +Y
        { Vec3f(0, -1, 0), Vec3f(1, 0, 0), Vec3f(0, 0, 1) }, // -Y
        { Vec3f(0, 0, 1), Vec3f(1, 0, 0), Vec3f(0, 1, 0) },  // +Z
        { Vec3f(0, 0, -1), Vec3f(1, 0, 0), Vec3f(0, 1, 0) }  // -Z
    };

    // Step 3: Process each face direction using greedy meshing
    const float voxelSize = 1.0f;
    const float halfVoxel = voxelSize * 0.5f;

    for (int faceIndex = 0; faceIndex < 6; faceIndex++)
    {
        Face face = faces[faceIndex];

        // Find bounds in face-local coordinates
        int minU = INT_MAX, minV = INT_MAX;
        int maxU = INT_MIN, maxV = INT_MIN;
        int minD = INT_MAX, maxD = INT_MIN;

        for (uint32 i = 0; i < voxelPositions.Size(); i++)
        {
            Vec3i pos = voxelPositions[i];

            // Project voxel position into face-local coordinates
            int u = (int)(pos.x * face.uDir.x + pos.y * face.uDir.y + pos.z * face.uDir.z);
            int v = (int)(pos.x * face.vDir.x + pos.y * face.vDir.y + pos.z * face.vDir.z);
            int d = (int)(pos.x * face.normal.x + pos.y * face.normal.y + pos.z * face.normal.z);

            // Track bounds
            minU = MathUtil::Min(minU, u);
            maxU = MathUtil::Max(maxU, u);
            minV = MathUtil::Min(minV, v);
            maxV = MathUtil::Max(maxV, v);
            minD = MathUtil::Min(minD, d);
            maxD = MathUtil::Max(maxD, d);
        }

        // Process each layer of voxels perpendicular to the face direction
        for (int d = minD; d <= maxD + 1; d++)
        {
            int width = maxU - minU + 1;
            int height = maxV - minV + 1;

            // Create a mask of visible faces for this layer
            Bitset visibleFaces;

            // Determine which faces are visible
            for (int v = 0; v < height; v++)
            {
                for (int u = 0; u < width; u++)
                {
                    // Convert back to world coordinates
                    Vec3i voxelPos;
                    voxelPos.x = (minU + u) * (int)face.uDir.x + (minV + v) * (int)face.vDir.x + d * (int)face.normal.x;
                    voxelPos.y = (minU + u) * (int)face.uDir.y + (minV + v) * (int)face.vDir.y + d * (int)face.normal.y;
                    voxelPos.z = (minU + u) * (int)face.uDir.z + (minV + v) * (int)face.vDir.z + d * (int)face.normal.z;

                    bool voxelExists = voxelSet.Contains(voxelPos);

                    Vec3i neighborPos = voxelPos;

                    if (face.normal.x + face.normal.y + face.normal.z > 0)
                    {
                        neighborPos.x -= (int)face.normal.x;
                        neighborPos.y -= (int)face.normal.y;
                        neighborPos.z -= (int)face.normal.z;
                    }
                    else
                    {
                        neighborPos.x += (int)face.normal.x;
                        neighborPos.y += (int)face.normal.y;
                        neighborPos.z += (int)face.normal.z;
                    }

                    bool neighborExists = voxelSet.Contains(neighborPos);

                    visibleFaces.Set(u + v * width, voxelExists && !neighborExists);
                }
            }

            // greedy meshing
            for (int v = 0; v < height; v++)
            {
                for (int u = 0; u < width;)
                {
                    if (!visibleFaces.Test(u + v * width))
                    {
                        u++;
                        continue;
                    }

                    // Find width of rectangle (how far we can go in u direction)
                    int rectWidth;
                    for (rectWidth = 1; (u + rectWidth) < width && visibleFaces.Test(u + rectWidth + v * width); rectWidth++)
                        ;

                    // Find height of rectangle (how far we can go in v direction)
                    int rectHeight;
                    for (rectHeight = 1; (v + rectHeight) < height; rectHeight++)
                    {
                        // Check if entire row is visible
                        bool rowIsVisible = true;
                        for (int du = 0; du < rectWidth; du++)
                        {
                            if (!visibleFaces.Test(u + du + (v + rectHeight) * width))
                            {
                                rowIsVisible = false;
                                break;
                            }
                        }
                        if (!rowIsVisible)
                            break;
                    }

                    // Mark all faces in this rectangle as processed
                    for (int dv = 0; dv < rectHeight; dv++)
                    {
                        for (int du = 0; du < rectWidth; du++)
                        {
                            visibleFaces.Set((u + du) + (v + dv) * width, false);
                        }
                    }

                    // Generate quad for this rectangle
                    Vec3f origin = face.uDir * (float)(minU + u) + face.vDir * (float)(minV + v) + face.normal * (float)d;
                    Vec3f uExtent = face.uDir * (float)rectWidth;
                    Vec3f vExtent = face.vDir * (float)rectHeight;
                    Vec3f offset = face.normal * halfVoxel;

                    // Calculate the four corners of the quad
                    Vec3f v0 = origin + offset;
                    Vec3f v1 = origin + uExtent + offset;
                    Vec3f v2 = origin + uExtent + vExtent + offset;
                    Vec3f v3 = origin + vExtent + offset;

                    Vec2f uv0(0, 0), uv1(1, 0), uv2(1, 1), uv3(0, 1);

                    // Add the quad to the mesh
                    vertices.PushBack(Vertex(v0, uv0, face.normal));
                    vertices.PushBack(Vertex(v1, uv1, face.normal));
                    vertices.PushBack(Vertex(v2, uv2, face.normal));
                    vertices.PushBack(Vertex(v3, uv3, face.normal));

                    // Add indices for the quad (two triangles)
                    indices.PushBack(vertexOffset + 0);
                    indices.PushBack(vertexOffset + 1);
                    indices.PushBack(vertexOffset + 2);

                    indices.PushBack(vertexOffset + 0);
                    indices.PushBack(vertexOffset + 2);
                    indices.PushBack(vertexOffset + 3);

                    vertexOffset += 4;

                    // Move to the next position in the row
                    u += rectWidth;
                }
            }
        }
    }

    MeshData meshData;
    meshData.desc.numVertices = (uint32)vertices.Size();
    meshData.desc.numIndices = (uint32)indices.Size();
    meshData.vertexData = std::move(vertices);
    meshData.indexData.SetSize(indices.Size() * sizeof(uint32));
    meshData.indexData.Write(indices.Size() * sizeof(uint32), 0, indices.Data());

    meshData.CalculateNormals(true);
    meshData.CalculateTangents();

    Handle<Mesh> mesh = CreateObject<Mesh>();
    mesh->SetMeshData(meshData);
    mesh->SetName(NAME("MeshBuilder_VoxelMesh"));

    return mesh;
}

} // namespace hyperion
