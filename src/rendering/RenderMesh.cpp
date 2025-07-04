/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderMesh.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <rendering/backend/RenderBackend.hpp>
#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <scene/Mesh.hpp>

#include <core/threading/Task.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

#include <unordered_map>
#include <cstring>

namespace hyperion {

#pragma region RenderMesh

RenderMesh::RenderMesh(Mesh* mesh)
    : m_mesh(mesh),
      m_numIndices(0)
{
}

RenderMesh::RenderMesh(RenderMesh&& other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase&&>(other)),
      m_mesh(other.m_mesh),
      m_vertexAttributes(other.m_vertexAttributes),
      m_streamedMeshData(std::move(other.m_streamedMeshData)),
      m_vbo(std::move(other.m_vbo)),
      m_ibo(std::move(other.m_ibo)),
      m_numIndices(other.m_numIndices)
{
    other.m_mesh = nullptr;
}

RenderMesh::~RenderMesh() = default;

void RenderMesh::Initialize_Internal()
{
    HYP_SCOPE;

    Assert(m_mesh != nullptr);

    UploadMeshData();
}

void RenderMesh::Destroy_Internal()
{
    HYP_SCOPE;

    SafeRelease(std::move(m_vbo));
    SafeRelease(std::move(m_ibo));

    m_streamedMeshDataHandle.Reset();
}

void RenderMesh::Update_Internal()
{
    HYP_SCOPE;

    Assert(m_mesh != nullptr);

    UploadMeshData();
}

void RenderMesh::UploadMeshData()
{
    HYP_SCOPE;

    HYP_LOG(Rendering, Debug, "Uploading mesh data: {}", m_mesh->Id());

    // upload mesh data
    Array<float> vertexBuffer;
    Array<uint32> indexBuffer;

    if (m_streamedMeshData != nullptr)
    {
        if (!m_streamedMeshDataHandle)
        {
            m_streamedMeshDataHandle = ResourceHandle(*m_streamedMeshData);
        }

        const MeshData& meshData = m_streamedMeshData->GetMeshData();

        vertexBuffer = BuildVertexBuffer(m_vertexAttributes, meshData);
        indexBuffer = meshData.indices;

        m_streamedMeshDataHandle.Reset();
    }

    // Ensure vertex buffer is not empty
    if (vertexBuffer.Empty())
    {
        vertexBuffer.Resize(1);
    }

    // Ensure indices exist and are a multiple of 3
    if (indexBuffer.Empty())
    {
        indexBuffer.Resize(3);
    }
    else if (indexBuffer.Size() % 3 != 0)
    {
        indexBuffer.Resize(indexBuffer.Size() + (3 - (indexBuffer.Size() % 3)));
    }

    m_numIndices = indexBuffer.Size();

    const SizeType packedBufferSize = vertexBuffer.ByteSize();
    const SizeType packedIndicesSize = indexBuffer.ByteSize();

    if (!m_vbo.IsValid() || m_vbo->Size() != packedBufferSize)
    {
        SafeRelease(std::move(m_vbo));

        m_vbo = g_renderBackend->MakeGpuBuffer(GpuBufferType::MESH_VERTEX_BUFFER, packedBufferSize);

#ifdef HYP_DEBUG_MODE
        m_vbo->SetDebugName(NAME_FMT("RenderMesh_VertexBuffer_{}", m_mesh->Id().Value()));
#endif
    }

    if (!m_vbo->IsCreated())
    {
        HYPERION_ASSERT_RESULT(m_vbo->Create());
    }

    if (!m_ibo.IsValid() || m_ibo->Size() != packedIndicesSize)
    {
        SafeRelease(std::move(m_ibo));

        m_ibo = g_renderBackend->MakeGpuBuffer(GpuBufferType::MESH_INDEX_BUFFER, packedIndicesSize);

#ifdef HYP_DEBUG_MODE
        m_ibo->SetDebugName(NAME_FMT("RenderMesh_IndexBuffer_{}", m_mesh->Id().Value()));
#endif
    }

    if (!m_ibo->IsCreated())
    {
        HYPERION_ASSERT_RESULT(m_ibo->Create());
    }

    GpuBufferRef stagingBufferVertices = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedBufferSize);
    HYPERION_ASSERT_RESULT(stagingBufferVertices->Create());
    stagingBufferVertices->Copy(packedBufferSize, vertexBuffer.Data());

    GpuBufferRef stagingBufferIndices = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedIndicesSize);
    HYPERION_ASSERT_RESULT(stagingBufferIndices->Create());
    stagingBufferIndices->Copy(packedIndicesSize, indexBuffer.Data());

    SingleTimeCommands commands;

    commands.Push([&](CmdList& cmd)
        {
            cmd.Add<CopyBuffer>(stagingBufferVertices, m_vbo, packedBufferSize);
        });

    commands.Push([&](CmdList& cmd)
        {
            cmd.Add<CopyBuffer>(stagingBufferIndices, m_ibo, packedIndicesSize);
        });

    HYPERION_ASSERT_RESULT(commands.Execute());

    stagingBufferVertices->Destroy();
    stagingBufferIndices->Destroy();
}

void RenderMesh::SetVertexAttributes(const VertexAttributeSet& vertexAttributes)
{
    HYP_SCOPE;

    Execute([this, vertexAttributes]()
        {
            m_vertexAttributes = vertexAttributes;

            if (IsInitialized())
            {
                SetNeedsUpdate();
            }
        });
}

void RenderMesh::SetStreamedMeshData(const RC<StreamedMeshData>& streamedMeshData)
{
    HYP_SCOPE;

    Execute([this, streamedMeshData, streamedMeshDataHandle = streamedMeshData ? ResourceHandle(*streamedMeshData) : ResourceHandle()]()
        {
            m_streamedMeshData = streamedMeshData;

            m_streamedMeshDataHandle = std::move(streamedMeshDataHandle);

            if (IsInitialized())
            {
                SetNeedsUpdate();
            }
        });
}

/* Copy our values into the packed vertex buffer, and increase the index for the next possible
 * mesh attribute. This macro helps keep the code cleaner and easier to maintain. */
#define PACKED_SET_ATTR(rawValues, argSize)                                                         \
    do                                                                                              \
    {                                                                                               \
        Memory::MemCpy((void*)(rawBuffer + currentOffset), (rawValues), (argSize) * sizeof(float)); \
        currentOffset += (argSize);                                                                 \
    }                                                                                               \
    while (0)

Array<float> RenderMesh::BuildVertexBuffer(const VertexAttributeSet& vertexAttributes, const MeshData& meshData)
{
    const SizeType vertexSize = vertexAttributes.CalculateVertexSize();

    Array<float> packedBuffer;
    packedBuffer.Resize(vertexSize * meshData.vertices.Size());

    /* Raw buffer that is used with our helper macro. */
    float* rawBuffer = packedBuffer.Data();
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

Array<PackedVertex> RenderMesh::BuildPackedVertices() const
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

Array<uint32> RenderMesh::BuildPackedIndices() const
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

BLASRef RenderMesh::BuildBLAS(const Handle<Material>& material) const
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

        virtual ~RENDER_COMMAND(BuildBLAS)() override = default;

        virtual RendererResult operator()() override
        {
            HYP_DEFER({
                SafeRelease(std::move(packedVerticesBuffer));
                SafeRelease(std::move(packedIndicesBuffer));
                SafeRelease(std::move(verticesStagingBuffer));
                SafeRelease(std::move(indicesStagingBuffer));
            });

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

            SingleTimeCommands commands;

            commands.Push([&](CmdList& cmd)
                {
                    cmd.Add<CopyBuffer>(verticesStagingBuffer, packedVerticesBuffer, packedVerticesSize);
                    cmd.Add<CopyBuffer>(indicesStagingBuffer, packedIndicesBuffer, packedIndicesSize);
                });

            commands.Push([&](CmdList&)
                {
                    blas->Create();
                });

            return commands.Execute();
        }
    };

    BLASRef blas;
    PUSH_RENDER_COMMAND(BuildBLAS, blas, std::move(packedVertices), std::move(packedIndices), material);

    return blas;
}

void RenderMesh::Render(CmdList& cmd, uint32 numInstances, uint32 instanceIndex) const
{
    cmd.Add<BindVertexBuffer>(GetVertexBuffer());
    cmd.Add<BindIndexBuffer>(GetIndexBuffer());
    cmd.Add<DrawIndexed>(NumIndices(), numInstances, instanceIndex);
}

void RenderMesh::RenderIndirect(CmdList& cmd, const GpuBufferRef& indirectBuffer, uint32 bufferOffset) const
{
    cmd.Add<BindVertexBuffer>(GetVertexBuffer());
    cmd.Add<BindIndexBuffer>(GetIndexBuffer());
    cmd.Add<DrawIndexedIndirect>(indirectBuffer, bufferOffset);
}

void RenderMesh::PopulateIndirectDrawCommand(IndirectDrawCommand& out)
{
#if HYP_VULKAN
    out.command = {
        .indexCount = NumIndices()
    };
#else
#error Not implemented for this platform!
#endif
}

#pragma endregion RenderMesh

} // namespace hyperion