/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/Mesh.hpp>

#include <rendering/RenderMesh.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scene/BVH.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

#include <cstring>

namespace hyperion {

#pragma region Mesh

Pair<Array<Vertex>, Array<uint32>> Mesh::CalculateIndices(const Array<Vertex>& vertices)
{
    HashMap<Vertex, uint32> indexMap;

    Array<uint32> indices;
    indices.Reserve(vertices.Size());

    /* This will be our resulting buffer with only the vertices we need. */
    Array<Vertex> newVertices;
    newVertices.Reserve(vertices.Size());

    for (const auto& vertex : vertices)
    {
        /* Check if the vertex already exists in our map */
        auto it = indexMap.Find(vertex);

        /* If it does, push to our indices */
        if (it != indexMap.End())
        {
            indices.PushBack(it->second);

            continue;
        }

        const uint32 meshIndex = uint32(newVertices.Size());

        /* The vertex is unique, so we push it. */
        newVertices.PushBack(vertex);
        indices.PushBack(meshIndex);

        indexMap[vertex] = meshIndex;
    }

    return { std::move(newVertices), std::move(indices) };
}

Mesh::Mesh()
    : HypObject(),
      m_meshAttributes { .vertexAttributes = staticMeshVertexAttributes, .topology = TOP_TRIANGLES },
      m_aabb(BoundingBox::Empty()),
      m_renderResource(nullptr)
{
}

Mesh::Mesh(RC<StreamedMeshData> streamedMeshData, Topology topology, const VertexAttributeSet& vertexAttributes)
    : HypObject(),
      m_meshAttributes { .vertexAttributes = vertexAttributes, .topology = topology },
      m_streamedMeshData(std::move(streamedMeshData)),
      m_aabb(BoundingBox::Empty()),
      m_renderResource(nullptr)
{
    if (m_streamedMeshData != nullptr)
    {
        m_streamedMeshDataResourceHandle = ResourceHandle(*m_streamedMeshData);
    }

    CalculateAABB();
}

Mesh::Mesh(RC<StreamedMeshData> streamedMeshData, Topology topology)
    : Mesh(std::move(streamedMeshData), topology, staticMeshVertexAttributes | skeletonVertexAttributes)
{
}

Mesh::Mesh(Array<Vertex> vertices, Array<uint32> indices, Topology topology)
    : Mesh(
          std::move(vertices),
          std::move(indices),
          topology,
          staticMeshVertexAttributes | skeletonVertexAttributes)
{
}

Mesh::Mesh(
    Array<Vertex> vertices,
    Array<uint32> indices,
    Topology topology,
    const VertexAttributeSet& vertexAttributes)
    : HypObject(),
      m_meshAttributes { .vertexAttributes = vertexAttributes, .topology = topology },
      m_streamedMeshData(
          MakeRefCountedPtr<StreamedMeshData>(
              MeshData { std::move(vertices), std::move(indices) },
              m_streamedMeshDataResourceHandle)),
      m_aabb(BoundingBox::Empty()),
      m_renderResource(nullptr)
{
    CalculateAABB();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_meshAttributes(other.m_meshAttributes),
      m_streamedMeshData(std::move(other.m_streamedMeshData)),
      m_streamedMeshDataResourceHandle(std::move(other.m_streamedMeshDataResourceHandle)),
      m_aabb(other.m_aabb),
      m_renderResource(other.m_renderResource)
{
    other.m_aabb = BoundingBox::Empty();
    other.m_renderResource = nullptr;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (&other == this)
    {
        return *this;
    }

    if (m_renderResource != nullptr)
    {
        FreeResource(m_renderResource);
    }

    m_meshAttributes = other.m_meshAttributes;
    m_streamedMeshData = std::move(other.m_streamedMeshData);
    m_streamedMeshDataResourceHandle = std::move(other.m_streamedMeshDataResourceHandle);
    m_aabb = other.m_aabb;
    m_renderResource = other.m_renderResource;

    other.m_aabb = BoundingBox::Empty();
    other.m_renderResource = nullptr;

    return *this;
}

Mesh::~Mesh()
{
    if (IsInitCalled())
    {
        SetReady(false);

        m_renderPersistent.Reset();

        if (m_renderResource != nullptr)
        {
            FreeResource(m_renderResource);
        }
    }

    m_streamedMeshDataResourceHandle.Reset();

    // @NOTE Must be after we free the m_renderResource,
    // since m_renderResource would be using our streamed mesh data
    if (m_streamedMeshData != nullptr)
    {
        m_streamedMeshData->WaitForFinalization();
        m_streamedMeshData.Reset();
    }
}

void Mesh::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind(
        [this]()
        {
            m_renderPersistent.Reset();

            if (m_renderResource != nullptr)
            {
                FreeResource(m_renderResource);

                m_renderResource = nullptr;
            }

            m_streamedMeshDataResourceHandle.Reset();

            if (m_streamedMeshData != nullptr)
            {
                m_streamedMeshData->WaitForFinalization();
                m_streamedMeshData.Reset();
            }
        }));

    Assert(GetVertexAttributes() != 0, "No vertex attributes set on mesh");

    m_renderResource = AllocateResource<RenderMesh>(this);

    {
        HYP_MT_CHECK_RW(m_dataRaceDetector, "Streamed mesh data");

        if (!m_streamedMeshData)
        {
            HYP_LOG(Mesh, Warning, "Creating empty streamed mesh data for mesh {}", Id().Value());

            m_streamedMeshData.Emplace();
        }

        m_renderResource->SetVertexAttributes(GetVertexAttributes());
        m_renderResource->SetStreamedMeshData(m_streamedMeshData);

        // Data passed to render resource to be uploaded,
        // Reset the resource handle now that we no longer need it in CPU-side memory
        HYP_LOG(Mesh, Debug, "Resetting streamed mesh data resource handle for mesh {}", Id().Value());
        m_streamedMeshDataResourceHandle.Reset();
    }

    SetReady(true);
}

void Mesh::SetVertices(Span<const Vertex> vertices)
{
    Array<uint32> indices;
    indices.Resize(vertices.Size());

    for (SizeType index = 0; index < vertices.Size(); index++)
    {
        indices[index] = uint32(index);
    }

    ResourceHandle tmp;

    SetStreamedMeshData(
        MakeRefCountedPtr<StreamedMeshData>(MeshData { Array<Vertex>(vertices), std::move(indices) }, tmp));
}

void Mesh::SetVertices(Span<const Vertex> vertices, Span<const uint32> indices)
{
    ResourceHandle tmp;

    SetStreamedMeshData(
        MakeRefCountedPtr<StreamedMeshData>(MeshData { Array<Vertex>(vertices), Array<uint32>(indices) }, tmp));
}

const RC<StreamedMeshData>& Mesh::GetStreamedMeshData() const
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_dataRaceDetector, "Streamed mesh data");

    return m_streamedMeshData;
}

void Mesh::SetStreamedMeshData(RC<StreamedMeshData> streamedMeshData)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector, "Streamed mesh data");

    if (m_streamedMeshData == streamedMeshData)
    {
        return;
    }

    m_streamedMeshDataResourceHandle.Reset();

    if (m_streamedMeshData != nullptr)
    {
        // Set render resource's streamed mesh data to null first
        // -- FreeResource will wait for usage to finish
        if (m_renderResource != nullptr)
        {
            m_renderResource->SetStreamedMeshData(nullptr);
        }

        m_streamedMeshData->WaitForFinalization();
    }

    m_streamedMeshData = std::move(streamedMeshData);

    CalculateAABB();

    if (IsInitCalled())
    {
        if (!m_streamedMeshData)
        {
            // Create empty streamed data if set to null.
            m_streamedMeshData = MakeRefCountedPtr<StreamedMeshData>(
                MeshData { Array<Vertex> {}, Array<uint32> {} }, m_streamedMeshDataResourceHandle);
        }

        m_renderResource->SetStreamedMeshData(m_streamedMeshData);
    }
}

uint32 Mesh::NumIndices() const
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_dataRaceDetector, "Streamed mesh data");

    return m_streamedMeshData != nullptr ? m_streamedMeshData->NumIndices() : 0;
}

void Mesh::SetVertexAttributes(const VertexAttributeSet& vertexAttributes)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector, "Attributes");

    m_meshAttributes.vertexAttributes = vertexAttributes;

    if (IsInitCalled())
    {
        m_renderResource->SetVertexAttributes(vertexAttributes);
    }
}

void Mesh::SetMeshAttributes(const MeshAttributes& attributes)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector, "Attributes");

    m_meshAttributes = attributes;

    if (IsInitCalled())
    {
        m_renderResource->SetVertexAttributes(attributes.vertexAttributes);
    }
}

void Mesh::SetPersistentRenderResourceEnabled(bool enabled)
{
    AssertIsInitCalled();

    HYP_MT_CHECK_RW(m_dataRaceDetector, "m_render_persistent");

    if (enabled)
    {
        if (!m_renderPersistent)
        {
            m_renderPersistent = ResourceHandle(*m_renderResource);
        }
    }
    else
    {
        m_renderPersistent.Reset();
    }
}

#define ADD_NORMAL(ary, idx, normal) \
    do { \
        auto* idx_it = ary.TryGet(idx); \
        if (!idx_it) \
        { \
            idx_it = &*ary.Emplace(idx); \
        } \
        idx_it->PushBack(normal); \
    } while (0)

void Mesh::CalculateNormals(bool weighted)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector, "Streamed mesh data");

    if (!m_streamedMeshData)
    {
        HYP_LOG(Mesh, Warning, "Cannot calculate normals before mesh data is set!");

        return;
    }

    if (!m_streamedMeshDataResourceHandle)
    {
        m_streamedMeshDataResourceHandle = ResourceHandle(*m_streamedMeshData);
    }

    if (!m_streamedMeshDataResourceHandle)
    {
        HYP_LOG(Mesh, Warning, "Cannot calculate normals, failed to get streamed mesh data resource handle!");

        return;
    }

    if (m_streamedMeshData->GetMeshData().indices.Empty())
    {
        HYP_LOG(Mesh, Warning, "Cannot calculate normals before indices are generated!");

        return;
    }

    MeshData meshData = m_streamedMeshData->GetMeshData();

    SparsePagedArray<Array<Vec3f, InlineAllocator<3>>, 1 << 6> normals;

    // compute per-face normals (facet normals)
    for (SizeType i = 0; i < meshData.indices.Size(); i += 3)
    {
        const uint32 i0 = meshData.indices[i];
        const uint32 i1 = meshData.indices[i + 1];
        const uint32 i2 = meshData.indices[i + 2];

        const Vec3f& p0 = meshData.vertices[i0].GetPosition();
        const Vec3f& p1 = meshData.vertices[i1].GetPosition();
        const Vec3f& p2 = meshData.vertices[i2].GetPosition();

        const Vec3f u = p2 - p0;
        const Vec3f v = p1 - p0;
        const Vec3f n = v.Cross(u).Normalize();

        ADD_NORMAL(normals, i0, n);
        ADD_NORMAL(normals, i1, n);
        ADD_NORMAL(normals, i2, n);
    }

    for (SizeType i = 0; i < meshData.vertices.Size(); i++)
    {
        AssertDebug(normals.HasIndex(uint32(i)));

        if (weighted)
        {
            meshData.vertices[i].SetNormal(normals.Get(uint32(i)).Sum());
        }
        else
        {
            meshData.vertices[i].SetNormal(normals.Get(uint32(i)).Sum().Normalize());
        }
    }

    if (!weighted)
    {
        m_streamedMeshDataResourceHandle.Reset();

        m_streamedMeshData->WaitForFinalization();
        m_streamedMeshData.Emplace(std::move(meshData), m_streamedMeshDataResourceHandle);

        return;
    }

    normals.Clear();

    // weighted (smooth) normals

    for (SizeType i = 0; i < meshData.indices.Size(); i += 3)
    {
        const uint32 i0 = meshData.indices[i];
        const uint32 i1 = meshData.indices[i + 1];
        const uint32 i2 = meshData.indices[i + 2];

        const Vec3f& p0 = meshData.vertices[i0].GetPosition();
        const Vec3f& p1 = meshData.vertices[i1].GetPosition();
        const Vec3f& p2 = meshData.vertices[i2].GetPosition();

        const Vec3f& n0 = meshData.vertices[i0].GetNormal();
        const Vec3f& n1 = meshData.vertices[i1].GetNormal();
        const Vec3f& n2 = meshData.vertices[i2].GetNormal();

        // Vector3 n = FixedArray { n0, n1, n2 }.Avg();

        FixedArray<Vec3f, 3> weightedNormals { n0, n1, n2 };

        // nested loop through faces to get weighted neighbours
        // any code that uses this really should bake the normals in
        // especially for any production code. this is an expensive process
        for (SizeType j = 0; j < meshData.indices.Size(); j += 3)
        {
            if (j == i)
            {
                continue;
            }

            const uint32 j0 = meshData.indices[j];
            const uint32 j1 = meshData.indices[j + 1];
            const uint32 j2 = meshData.indices[j + 2];

            const FixedArray<Vec3f, 3> facePositions { meshData.vertices[j0].GetPosition(),
                meshData.vertices[j1].GetPosition(),
                meshData.vertices[j2].GetPosition() };

            const FixedArray<Vec3f, 3> faceNormals { meshData.vertices[j0].GetNormal(),
                meshData.vertices[j1].GetNormal(),
                meshData.vertices[j2].GetNormal() };

            const Vec3f a = p1 - p0;
            const Vec3f b = p2 - p0;
            const Vec3f c = a.Cross(b);

            const float area = 0.5f * MathUtil::Sqrt(c.Dot(c));

            if (facePositions.Contains(p0))
            {
                const float angle = (p0 - p1).AngleBetween(p0 - p2);
                weightedNormals[0] += faceNormals.Avg() * area * angle;
            }

            if (facePositions.Contains(p1))
            {
                const float angle = (p1 - p0).AngleBetween(p1 - p2);
                weightedNormals[1] += faceNormals.Avg() * area * angle;
            }

            if (facePositions.Contains(p2))
            {
                const float angle = (p2 - p0).AngleBetween(p2 - p1);
                weightedNormals[2] += faceNormals.Avg() * area * angle;
            }

            // if (facePositions.Contains(p0)) {
            //     weightedNormals[0] += faceNormals.Avg();
            // }

            // if (facePositions.Contains(p1)) {
            //     weightedNormals[1] += faceNormals.Avg();
            // }

            // if (facePositions.Contains(p2)) {
            //     weightedNormals[2] += faceNormals.Avg();
            // }
        }

        ADD_NORMAL(normals, i0, weightedNormals[0].Normalized());
        ADD_NORMAL(normals, i1, weightedNormals[1].Normalized());
        ADD_NORMAL(normals, i2, weightedNormals[2].Normalized());
    }

    for (SizeType i = 0; i < meshData.vertices.Size(); i++)
    {
        AssertDebug(normals.HasIndex(i));

        meshData.vertices[i].SetNormal(normals.Get(i).Sum().Normalized());
    }

    normals.Clear();

    m_streamedMeshDataResourceHandle.Reset();

    m_streamedMeshData->WaitForFinalization();
    m_streamedMeshData.Emplace(std::move(meshData), m_streamedMeshDataResourceHandle);
}

#undef ADD_NORMAL

#define ADD_TANGENTS(ary, idx, tangents) \
    do { \
        auto* idx_it = ary.TryGet(idx); \
        if (!idx_it) \
        { \
            idx_it = &*ary.Emplace(idx); \
        } \
        idx_it->PushBack(tangents); \
    } while (0)

void Mesh::CalculateTangents()
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector, "Streamed mesh data");

    if (!m_streamedMeshData)
    {
        HYP_LOG(Mesh, Warning, "Cannot calculate normals before mesh data is set!");

        return;
    }

    if (!m_streamedMeshDataResourceHandle)
    {
        m_streamedMeshDataResourceHandle = ResourceHandle(*m_streamedMeshData);
    }

    if (!m_streamedMeshDataResourceHandle)
    {
        HYP_LOG(Mesh, Warning, "Cannot calculate normals, failed to get streamed mesh data resource handle!");

        return;
    }

    MeshData meshData = m_streamedMeshData->GetMeshData();

    struct TangentBitangentPair
    {
        Vec3f tangent;
        Vec3f bitangent;
    };

    static const Array<TangentBitangentPair, InlineAllocator<1>> placeholderTangentBitangents {};

    SparsePagedArray<Array<TangentBitangentPair, InlineAllocator<1>>, 1 << 6> data;

    for (SizeType i = 0; i < meshData.indices.Size();)
    {
        const SizeType count = MathUtil::Min(3, meshData.indices.Size() - i);

        Vertex v[3];
        Vec2f uv[3];

        for (uint32 j = 0; j < count; j++)
        {
            v[j] = meshData.vertices[meshData.indices[i + j]];
            uv[j] = v[j].GetTexCoord0();
        }

        uint32 i0 = meshData.indices[i];
        uint32 i1 = meshData.indices[i + 1];
        uint32 i2 = meshData.indices[i + 2];

        const Vec3f edge1 = v[1].GetPosition() - v[0].GetPosition();
        const Vec3f edge2 = v[2].GetPosition() - v[0].GetPosition();
        const Vec2f edge1uv = uv[1] - uv[0];
        const Vec2f edge2uv = uv[2] - uv[0];

        const float cp = edge1uv.x * edge2uv.y - edge1uv.y * edge2uv.x;

        if (cp != 0.0f)
        {
            const float mul = 1.0f / cp;

            const TangentBitangentPair tangentBitangent {
                .tangent = ((edge1 * edge2uv.y - edge2 * edge1uv.y) * mul).Normalize(),
                .bitangent = ((edge1 * edge2uv.x - edge2 * edge1uv.x) * mul).Normalize()
            };

            ADD_TANGENTS(data, i0, tangentBitangent);
            ADD_TANGENTS(data, i1, tangentBitangent);
            ADD_TANGENTS(data, i2, tangentBitangent);
        }

        i += count;
    }

    for (SizeType i = 0; i < meshData.vertices.Size(); i++)
    {
        const Array<TangentBitangentPair, InlineAllocator<1>>* tangentBitangents = data.TryGet(i);

        if (!tangentBitangents)
        {
            tangentBitangents = &placeholderTangentBitangents;
        }

        // find average
        Vec3f averageTangent, averageBitangent;

        for (const auto& item : *tangentBitangents)
        {
            averageTangent += item.tangent * (1.0f / tangentBitangents->Size());
            averageBitangent += item.bitangent * (1.0f / tangentBitangents->Size());
        }

        averageTangent.Normalize();
        averageBitangent.Normalize();

        meshData.vertices[i].SetTangent(averageTangent);
        meshData.vertices[i].SetBitangent(averageBitangent);
    }

    m_meshAttributes.vertexAttributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT;
    m_meshAttributes.vertexAttributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT;

    m_streamedMeshDataResourceHandle.Reset();

    m_streamedMeshData->WaitForFinalization();
    m_streamedMeshData.Emplace(std::move(meshData), m_streamedMeshDataResourceHandle);
}

#undef ADD_TANGENTS

void Mesh::InvertNormals()
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector, "Streamed mesh data");

    if (!m_streamedMeshData)
    {
        HYP_LOG(Mesh, Warning, "Cannot invert normals before mesh data is set!");

        return;
    }

    if (!m_streamedMeshDataResourceHandle)
    {
        m_streamedMeshDataResourceHandle = ResourceHandle(*m_streamedMeshData);
    }

    MeshData meshData = m_streamedMeshData->GetMeshData();

    for (Vertex& vertex : meshData.vertices)
    {
        vertex.SetNormal(vertex.GetNormal() * -1.0f);
    }

    m_streamedMeshDataResourceHandle.Reset();

    m_streamedMeshData->WaitForFinalization();
    m_streamedMeshData.Emplace(std::move(meshData), m_streamedMeshDataResourceHandle);
}

void Mesh::CalculateAABB()
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_dataRaceDetector, "Streamed mesh data");

    if (!m_streamedMeshData)
    {
        HYP_LOG(Mesh, Warning, "Cannot calculate Mesh bounds before mesh data is set!");

        m_aabb = BoundingBox::Empty();

        return;
    }

    ResourceHandle resourceHandle(*m_streamedMeshData);

    const MeshData& meshData = m_streamedMeshData->GetMeshData();

    BoundingBox aabb = BoundingBox::Empty();

    for (const Vertex& vertex : meshData.vertices)
    {
        aabb = aabb.Union(vertex.GetPosition());
    }

    m_aabb = aabb;
}

bool Mesh::BuildBVH(int maxDepth)
{
    if (!m_streamedMeshData)
    {
        return false;
    }

    ResourceHandle resourceHandle(*m_streamedMeshData);

    const MeshData& meshData = m_streamedMeshData->GetMeshData();

    m_bvh = BVHNode(m_aabb);

    for (uint32 i = 0; i < meshData.indices.Size(); i += 3)
    {
        Triangle triangle { meshData.vertices[meshData.indices[i + 0]],
            meshData.vertices[meshData.indices[i + 1]],
            meshData.vertices[meshData.indices[i + 2]] };

        triangle[0].position = triangle[0].position;
        triangle[1].position = triangle[1].position;
        triangle[2].position = triangle[2].position;

        triangle[0].normal = (Vec4f(triangle[0].normal.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[1].normal = (Vec4f(triangle[1].normal.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[2].normal = (Vec4f(triangle[2].normal.Normalized(), 0.0f)).GetXYZ().Normalize();

        triangle[0].tangent = (Vec4f(triangle[0].tangent.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[1].tangent = (Vec4f(triangle[1].tangent.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[2].tangent = (Vec4f(triangle[2].tangent.Normalized(), 0.0f)).GetXYZ().Normalize();

        triangle[0].bitangent = (Vec4f(triangle[0].bitangent.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[1].bitangent = (Vec4f(triangle[1].bitangent.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[2].bitangent = (Vec4f(triangle[2].bitangent.Normalized(), 0.0f)).GetXYZ().Normalize();

        m_bvh.AddTriangle(triangle);
    }

    m_bvh.Split(maxDepth);

    return true;
}

#pragma endregion Mesh

} // namespace hyperion