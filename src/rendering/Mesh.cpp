/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/Mesh.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderCommand.hpp>

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
      m_aabb(BoundingBox::Empty())
{
}

Mesh::Mesh(RC<StreamedMeshData> streamedMeshData, Topology topology, const VertexAttributeSet& vertexAttributes)
    : HypObject(),
      m_meshAttributes { .vertexAttributes = vertexAttributes, .topology = topology },
      m_streamedMeshData(std::move(streamedMeshData)),
      m_aabb(BoundingBox::Empty())
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
      m_aabb(BoundingBox::Empty())
{
    CalculateAABB();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_meshAttributes(other.m_meshAttributes),
      m_streamedMeshData(std::move(other.m_streamedMeshData)),
      m_streamedMeshDataResourceHandle(std::move(other.m_streamedMeshDataResourceHandle)),
      m_aabb(other.m_aabb)
{
    other.m_aabb = BoundingBox::Empty();
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (&other == this)
    {
        return *this;
    }

    m_meshAttributes = other.m_meshAttributes;
    m_streamedMeshData = std::move(other.m_streamedMeshData);
    m_streamedMeshDataResourceHandle = std::move(other.m_streamedMeshDataResourceHandle);
    m_aabb = other.m_aabb;

    other.m_aabb = BoundingBox::Empty();

    return *this;
}

Mesh::~Mesh()
{
    if (IsInitCalled())
    {
        SetReady(false);
    }

    m_streamedMeshDataResourceHandle.Reset();

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
            m_streamedMeshDataResourceHandle.Reset();

            if (m_streamedMeshData != nullptr)
            {
                m_streamedMeshData->WaitForFinalization();
                m_streamedMeshData.Reset();
            }
        }));

    Assert(GetVertexAttributes() != 0, "No vertex attributes set on mesh");

    {
        HYP_MT_CHECK_RW(m_dataRaceDetector, "Streamed mesh data");

        if (!m_streamedMeshData)
        {
            HYP_LOG(Mesh, Warning, "Creating empty streamed mesh data for mesh {}", Id().Value());

            m_streamedMeshData.Emplace();
        }

        CreateGpuBuffers();
    }

    m_streamedMeshDataResourceHandle.Reset();

    SetReady(true);
}

void Mesh::CreateGpuBuffers()
{
    Array<float> vertices;
    Array<uint32> indices;

    if (m_streamedMeshData != nullptr)
    {
        ResourceHandle streamedMeshDataHandle = m_streamedMeshDataResourceHandle;

        if (!streamedMeshDataHandle)
        {
            streamedMeshDataHandle = ResourceHandle(*m_streamedMeshData);
        }

        const MeshData& meshData = m_streamedMeshData->GetMeshData();

        vertices = BuildVertexBuffer(m_meshAttributes.vertexAttributes, meshData);
        indices = meshData.indices;
    }

    // Ensure vertex buffer is not empty
    if (vertices.Empty())
    {
        vertices.Resize(1);
    }

    // Ensure indices exist and are a multiple of 3
    if (indices.Empty())
    {
        indices.Resize(3);
    }
    else if (indices.Size() % 3 != 0)
    {
        indices.Resize(indices.Size() + (3 - (indices.Size() % 3)));
    }

    const SizeType packedBufferSize = vertices.ByteSize();
    const SizeType packedIndicesSize = indices.ByteSize();

    if (!m_vertexBuffer.IsValid() || m_vertexBuffer->Size() != packedBufferSize)
    {
        SafeRelease(std::move(m_vertexBuffer));

        m_vertexBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::MESH_VERTEX_BUFFER, packedBufferSize);

#ifdef HYP_DEBUG_MODE
        m_vertexBuffer->SetDebugName(NAME_FMT("Mesh_VertexBuffer_{}", Id().Value()));
#endif
    }

    DeferCreate(m_vertexBuffer);

    if (!m_indexBuffer.IsValid() || m_indexBuffer->Size() != packedIndicesSize)
    {
        SafeRelease(std::move(m_indexBuffer));

        m_indexBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::MESH_INDEX_BUFFER, packedIndicesSize);

#ifdef HYP_DEBUG_MODE
        m_indexBuffer->SetDebugName(NAME_FMT("Mesh_IndexBuffer_{}", Id().Value()));
#endif
    }

    DeferCreate(m_indexBuffer);

    struct RENDER_COMMAND(CopyMeshGpuData)
        : public RenderCommand
    {
        Array<float> vertices;
        Array<uint32> indices;

        GpuBufferRef vertexBuffer;
        GpuBufferRef indexBuffer;

        RENDER_COMMAND(CopyMeshGpuData)(Array<float>&& vertices, Array<uint32>&& indices, const GpuBufferRef& vertexBuffer, const GpuBufferRef& indexBuffer)
            : vertices(std::move(vertices)),
              indices(std::move(indices)),
              vertexBuffer(vertexBuffer),
              indexBuffer(indexBuffer)
        {
        }

        virtual ~RENDER_COMMAND(CopyMeshGpuData)() override
        {
            SafeRelease(std::move(vertexBuffer));
            SafeRelease(std::move(indexBuffer));
        }

        virtual RendererResult operator()() override
        {
            const SizeType packedBufferSize = vertices.ByteSize();
            const SizeType packedIndicesSize = indices.ByteSize();

            GpuBufferRef stagingBufferVertices = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedBufferSize);
            HYPERION_ASSERT_RESULT(stagingBufferVertices->Create());
            stagingBufferVertices->Copy(packedBufferSize, vertices.Data());

            GpuBufferRef stagingBufferIndices = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedIndicesSize);
            HYPERION_ASSERT_RESULT(stagingBufferIndices->Create());
            stagingBufferIndices->Copy(packedIndicesSize, indices.Data());

            FrameBase* frame = g_renderBackend->GetCurrentFrame();
            RenderQueue& renderQueue = frame->renderQueue;

            renderQueue << CopyBuffer(stagingBufferVertices, vertexBuffer, packedBufferSize);
            renderQueue << CopyBuffer(stagingBufferIndices, indexBuffer, packedIndicesSize);

            SafeRelease(std::move(stagingBufferVertices));
            SafeRelease(std::move(stagingBufferIndices));

            return {};
        }
    };

    PUSH_RENDER_COMMAND(CopyMeshGpuData, std::move(vertices), std::move(indices), m_vertexBuffer, m_indexBuffer);
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

        CreateGpuBuffers();
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

    if (m_meshAttributes.vertexAttributes == vertexAttributes)
    {
        return;
    }

    m_meshAttributes.vertexAttributes = vertexAttributes;

    if (IsInitCalled())
    {
        CreateGpuBuffers();
    }
}

void Mesh::SetMeshAttributes(const MeshAttributes& attributes)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector, "Attributes");

    if (m_meshAttributes == attributes)
    {
        return;
    }

    m_meshAttributes = attributes;

    if (IsInitCalled())
    {
        CreateGpuBuffers();
    }
}

/* Copy our values into the packed vertex buffer, and increase the index for the next possible
 * mesh attribute. This macro helps keep the code cleaner and easier to maintain. */
#define PACKED_SET_ATTR(rawValues, argSize)                                                           \
    do                                                                                                \
    {                                                                                                 \
        Memory::MemCpy((void*)(floatBuffer + currentOffset), (rawValues), (argSize) * sizeof(float)); \
        currentOffset += (argSize);                                                                   \
    }                                                                                                 \
    while (0)

Array<float> Mesh::BuildVertexBuffer(const VertexAttributeSet& vertexAttributes, const MeshData& meshData)
{
    const SizeType vertexSize = vertexAttributes.CalculateVertexSize();

    Array<float> packedBuffer;
    packedBuffer.Resize(vertexSize * meshData.vertices.Size());

    float* floatBuffer = packedBuffer.Data();
    SizeType currentOffset = 0;

    for (SizeType i = 0; i < meshData.vertices.Size(); i++)
    {
        const Vertex& vertex = meshData.vertices[i];
        /* Offset aligned to the current vertex */
        // currentOffset = i * vertexSize;

        /* Position and normals */
        if (vertexAttributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION)
            PACKED_SET_ATTR(vertex.GetPosition().values, 3);
        if (vertexAttributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL)
            PACKED_SET_ATTR(vertex.GetNormal().values, 3);
        /* Texture coordinates */
        if (vertexAttributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0)
            PACKED_SET_ATTR(vertex.GetTexCoord0().values, 2);
        if (vertexAttributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1)
            PACKED_SET_ATTR(vertex.GetTexCoord1().values, 2);
        /* Tangents and Bitangents */
        if (vertexAttributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT)
            PACKED_SET_ATTR(vertex.GetTangent().values, 3);
        if (vertexAttributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT)
            PACKED_SET_ATTR(vertex.GetBitangent().values, 3);

        /* TODO: modify GetBoneIndex/GetBoneWeight to return a Vector4. */
        if (vertexAttributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS)
        {
            float weights[4] = {
                vertex.GetBoneWeight(0), vertex.GetBoneWeight(1),
                vertex.GetBoneWeight(2), vertex.GetBoneWeight(3)
            };
            PACKED_SET_ATTR(weights, std::size(weights));
        }

        if (vertexAttributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES)
        {
            float indices[4] = {
                (float)vertex.GetBoneIndex(0), (float)vertex.GetBoneIndex(1),
                (float)vertex.GetBoneIndex(2), (float)vertex.GetBoneIndex(3)
            };
            PACKED_SET_ATTR(indices, std::size(indices));
        }
    }

    return packedBuffer;
}

#undef PACKED_SET_ATTR

Array<PackedVertex> Mesh::BuildPackedVertices() const
{
    HYP_SCOPE;

    if (!m_streamedMeshData)
    {
        return {};
    }

    ResourceHandle streamedMeshDataHandle(*m_streamedMeshData);

    const MeshData& meshData = m_streamedMeshData->GetMeshData();

    Array<PackedVertex> packedVertices;
    packedVertices.Resize(meshData.vertices.Size());

    for (SizeType i = 0; i < meshData.vertices.Size(); i++)
    {
        const auto& vertex = meshData.vertices[i];

        packedVertices[i] = PackedVertex {
            .positionX = vertex.GetPosition().x,
            .positionY = vertex.GetPosition().y,
            .positionZ = vertex.GetPosition().z,
            .normalX = vertex.GetNormal().x,
            .normalY = vertex.GetNormal().y,
            .normalZ = vertex.GetNormal().z,
            .texcoord0X = vertex.GetTexCoord0().x,
            .texcoord0Y = vertex.GetTexCoord0().y
        };
    }

    return packedVertices;
}

Array<uint32> Mesh::BuildPackedIndices() const
{
    HYP_SCOPE;

    if (!m_streamedMeshData)
    {
        return {};
    }

    ResourceHandle streamedMeshDataHandle(*m_streamedMeshData);

    const MeshData& meshData = m_streamedMeshData->GetMeshData();

    Assert(meshData.indices.Size() % 3 == 0);

    return Array<uint32>(meshData.indices.Begin(), meshData.indices.End());
}

BLASRef Mesh::BuildBLAS(const Handle<Material>& material) const
{
    Array<PackedVertex> packedVertices = BuildPackedVertices();
    Array<uint32> packedIndices = BuildPackedIndices();

    if (packedVertices.Empty() || packedIndices.Empty())
    {
        return nullptr;
    }

    // some assertions to prevent gpu faults down the line
    for (SizeType i = 0; i < packedIndices.Size(); i++)
    {
        Assert(packedIndices[i] < packedVertices.Size());
    }

    struct RENDER_COMMAND(BuildBLAS)
        : public RenderCommand
    {
        BLASRef blas;
        Array<PackedVertex> packedVertices;
        Array<uint32> packedIndices;
        Handle<Material> material;

        GpuBufferRef packedVerticesBuffer;
        GpuBufferRef packedIndicesBuffer;
        GpuBufferRef verticesStagingBuffer;
        GpuBufferRef indicesStagingBuffer;

        RENDER_COMMAND(BuildBLAS)(BLASRef& blas, Array<PackedVertex>&& packedVertices, Array<uint32>&& packedIndices, const Handle<Material>& material)
            : packedVertices(std::move(packedVertices)),
              packedIndices(std::move(packedIndices)),
              material(material)
        {
            const SizeType packedVerticesSize = this->packedVertices.Size() * sizeof(PackedVertex);
            const SizeType packedIndicesSize = this->packedIndices.Size() * sizeof(uint32);

            packedVerticesBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::RT_MESH_VERTEX_BUFFER, packedVerticesSize);
            packedIndicesBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::RT_MESH_INDEX_BUFFER, packedIndicesSize);

            blas = g_renderBackend->MakeBLAS(
                packedVerticesBuffer,
                packedIndicesBuffer,
                material,
                Matrix4::identity);

            this->blas = blas;
        }

        virtual ~RENDER_COMMAND(BuildBLAS)() override
        {
            SafeRelease(std::move(packedVerticesBuffer));
            SafeRelease(std::move(packedIndicesBuffer));
            SafeRelease(std::move(verticesStagingBuffer));
            SafeRelease(std::move(indicesStagingBuffer));
        }

        virtual RendererResult operator()() override
        {
            const SizeType packedVerticesSize = packedVertices.Size() * sizeof(PackedVertex);
            const SizeType packedIndicesSize = packedIndices.Size() * sizeof(uint32);

            HYPERION_BUBBLE_ERRORS(packedVerticesBuffer->Create());
            HYPERION_BUBBLE_ERRORS(packedIndicesBuffer->Create());

            verticesStagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedVerticesSize);
            HYPERION_BUBBLE_ERRORS(verticesStagingBuffer->Create());
            verticesStagingBuffer->Memset(packedVerticesSize, 0); // zero out
            verticesStagingBuffer->Copy(packedVertices.Size() * sizeof(PackedVertex), packedVertices.Data());

            indicesStagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedIndicesSize);
            HYPERION_BUBBLE_ERRORS(indicesStagingBuffer->Create());
            indicesStagingBuffer->Memset(packedIndicesSize, 0); // zero out
            indicesStagingBuffer->Copy(packedIndices.Size() * sizeof(uint32), packedIndices.Data());

            FrameBase* frame = g_renderBackend->GetCurrentFrame();
            RenderQueue& renderQueue = frame->renderQueue;

            renderQueue << CopyBuffer(verticesStagingBuffer, packedVerticesBuffer, packedVerticesSize);
            renderQueue << CopyBuffer(indicesStagingBuffer, packedIndicesBuffer, packedIndicesSize);

            return {};
        }
    };

    BLASRef blas;
    PUSH_RENDER_COMMAND(BuildBLAS, blas, std::move(packedVertices), std::move(packedIndices), material);

    return blas;
}

void Mesh::SetPersistentRenderResourceEnabled(bool enabled)
{
    AssertIsInitCalled();

    /// No longer used
}

#define ADD_NORMAL(ary, idx, normal)     \
    do                                   \
    {                                    \
        auto* idx_it = ary.TryGet(idx);  \
        if (!idx_it)                     \
        {                                \
            idx_it = &*ary.Emplace(idx); \
        }                                \
        idx_it->PushBack(normal);        \
    }                                    \
    while (0)

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
    do                                   \
    {                                    \
        auto* idx_it = ary.TryGet(idx);  \
        if (!idx_it)                     \
        {                                \
            idx_it = &*ary.Emplace(idx); \
        }                                \
        idx_it->PushBack(tangents);      \
    }                                    \
    while (0)

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