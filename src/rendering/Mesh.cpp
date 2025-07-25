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

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <scene/BVH.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

#include <cstring>

namespace hyperion {

static const Name g_nameMeshDefault = NAME("<unnamed mesh>");

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
      m_aabb(BoundingBox::Empty())
{
}

Mesh::Mesh(const Handle<MeshAsset>& asset, Topology topology, const VertexAttributeSet& vertexAttributes)
    : HypObject(),
      m_asset(asset),
      m_aabb(BoundingBox::Empty())
{
    if (m_asset.IsValid())
    {
        ResourceHandle resourceHandle(*m_asset->GetResource());

        m_aabb = m_asset->GetMeshData()->CalculateAABB();
    }
}

Mesh::Mesh(const Handle<MeshAsset>& asset, Topology topology)
    : Mesh(asset, topology, staticMeshVertexAttributes | skeletonVertexAttributes)
{
}

Mesh::Mesh(const Array<Vertex>& vertexData, const ByteBuffer& indexData, Topology topology)
    : Mesh(
          vertexData,
          indexData,
          topology,
          staticMeshVertexAttributes | skeletonVertexAttributes)
{
}

Mesh::Mesh(const Array<Vertex>& vertexData, const ByteBuffer& indexData, Topology topology, const VertexAttributeSet& vertexAttributes)
    : HypObject(),
      m_aabb(BoundingBox::Empty())
{
    const MeshDesc meshDesc {
        .meshAttributes = { .vertexAttributes = vertexAttributes, .topology = topology },
        .numVertices = uint32(vertexData.Size()),
        .numIndices = uint32(indexData.Size() / sizeof(uint32))
    };

    const MeshData meshData {
        .desc = meshDesc,
        .vertexData = vertexData,
        .indexData = indexData
    };

    m_aabb = m_asset->GetMeshData()->CalculateAABB();

    m_asset = CreateObject<MeshAsset>(g_nameMeshDefault, meshData);
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_asset(std::move(other.m_asset)),
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

    m_asset = std::move(other.m_asset);
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
}

void Mesh::Init()
{
    if (m_asset.IsValid() && !m_asset->IsRegistered())
    {
        if (!m_asset->GetName().IsValid())
        {
            m_asset->Rename(m_name);
        }

        // all assets must be registered before uploading to gpu - if our asset isn't part of a package,
        // register it with transient Memory package
        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", m_asset);
    }

    CreateGpuBuffers();

    SetReady(true);
}

void Mesh::CreateGpuBuffers()
{
    if (!m_asset)
    {
        HYP_LOG(Mesh, Error, "Mesh asset is not set, cannot create GPU buffers");

        return;
    }

    ResourceHandle resourceHandle(*m_asset->GetResource());

    AssertDebug(m_asset->GetMeshData()->desc.meshAttributes.vertexAttributes != 0, "No vertex attributes set on mesh");

    Array<float> vertices = m_asset->GetMeshData()->BuildVertexBuffer();

    Array<uint32> indices;
    indices.Resize(m_asset->GetMeshData()->indexData.Size() / sizeof(uint32));
    Memory::MemCpy(indices.Data(), m_asset->GetMeshData()->indexData.Data(), m_asset->GetMeshData()->indexData.Size());

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

void Mesh::SetName(Name name)
{
    if (m_name == name)
    {
        return;
    }

    m_name = name;

    if (m_asset.IsValid() && !m_asset->IsRegistered() && IsInitCalled())
    {
        if (!m_asset->GetName().IsValid())
        {
            m_asset->Rename(m_name);
        }

        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", m_asset);
    }
}

void Mesh::SetVertices(Span<const Vertex> vertices)
{
    Array<uint32> indices;
    indices.Resize(vertices.Size());

    for (SizeType index = 0; index < vertices.Size(); index++)
    {
        indices[index] = uint32(index);
    }

    const MeshDesc meshDesc {
        .meshAttributes = GetMeshAttributes(),
        .numVertices = uint32(vertices.Size()),
        .numIndices = uint32(indices.Size())
    };

    Handle<AssetPackage> package;
    MeshData meshData;

    if (m_asset.IsValid())
    {
        Handle<MeshAsset> prevAsset = m_asset;

        package = prevAsset->GetPackage();

        ResourceHandle resourceHandle(*prevAsset->GetResource());

        meshData = std::move(*prevAsset->GetMeshData());
    }

    meshData.desc = meshDesc;

    m_aabb = meshData.CalculateAABB();

    Handle<MeshAsset> newAsset = CreateObject<MeshAsset>(GetName(), meshData);

    if (package.IsValid())
    {
        package->RemoveAssetObject(m_asset);
        package->AddAssetObject(newAsset);
    }

    m_asset = std::move(newAsset);

    if (IsInitCalled())
    {
        CreateGpuBuffers();
    }
}

void Mesh::SetVertices(Span<const Vertex> vertices, Span<const uint32> indices)
{
    HYP_CORE_ASSERT(indices.Size() % 3 == 0, "Indices must be a multiple of 3");

    const MeshDesc meshDesc {
        .meshAttributes = GetMeshAttributes(),
        .numVertices = uint32(vertices.Size()),
        .numIndices = uint32(indices.Size())
    };

    Handle<AssetPackage> package;
    MeshData meshData;

    if (m_asset.IsValid())
    {
        Handle<MeshAsset> prevAsset = m_asset;

        package = prevAsset->GetPackage();

        ResourceHandle resourceHandle(*prevAsset->GetResource());

        meshData = std::move(*prevAsset->GetMeshData());
    }

    meshData.desc = meshDesc;
    meshData.vertexData = Array<Vertex>(vertices);
    meshData.indexData = ByteBuffer(ConstByteView(reinterpret_cast<const ubyte*>(indices.Data()), reinterpret_cast<const ubyte*>(indices.Data() + indices.Size())));

    m_aabb = meshData.CalculateAABB();

    Handle<MeshAsset> newAsset = CreateObject<MeshAsset>(GetName(), meshData);

    if (package.IsValid())
    {
        package->RemoveAssetObject(m_asset);
        package->AddAssetObject(newAsset);
    }

    m_asset = std::move(newAsset);

    if (IsInitCalled())
    {
        CreateGpuBuffers();
    }
}

void Mesh::SetMeshData(const MeshData& meshData)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    Handle<AssetPackage> package;

    if (m_asset.IsValid())
    {
        Handle<MeshAsset> prevAsset = m_asset;

        package = prevAsset->GetPackage();
    }

    m_aabb = meshData.CalculateAABB();

    Handle<MeshAsset> newAsset = CreateObject<MeshAsset>(GetName(), meshData);

    if (package.IsValid())
    {
        package->RemoveAssetObject(m_asset);
        package->AddAssetObject(newAsset);
    }

    m_asset = std::move(newAsset);

    if (IsInitCalled())
    {
        if (!m_asset->IsRegistered())
        {
            g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", m_asset);
        }

        CreateGpuBuffers();
    }
}

uint32 Mesh::NumIndices() const
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_dataRaceDetector, "Streamed mesh data");

    return m_asset ? m_asset->GetMeshDesc().numIndices : 0;
}

void Mesh::SetVertexAttributes(const VertexAttributeSet& vertexAttributes)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector, "Attributes");

    MeshDesc meshDesc = m_asset.IsValid() ? m_asset->GetMeshDesc() : MeshDesc();

    MeshAttributes meshAttributes = meshDesc.meshAttributes;
    meshAttributes.vertexAttributes = vertexAttributes;

    SetMeshAttributes(meshAttributes);
}

void Mesh::SetMeshAttributes(const MeshAttributes& attributes)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector, "Attributes");

    MeshDesc meshDesc = m_asset.IsValid() ? m_asset->GetMeshDesc() : MeshDesc();
    meshDesc.meshAttributes = attributes;

    Handle<AssetPackage> package;
    MeshData meshData;

    if (m_asset.IsValid())
    {
        Handle<MeshAsset> prevAsset = m_asset;

        package = prevAsset->GetPackage();

        ResourceHandle resourceHandle(*prevAsset->GetResource());

        meshData = std::move(*prevAsset->GetMeshData());
    }

    meshData.desc = meshDesc;

    m_aabb = meshData.CalculateAABB();

    Handle<MeshAsset> newAsset = CreateObject<MeshAsset>(GetName(), meshData);

    if (package.IsValid())
    {
        package->RemoveAssetObject(m_asset);
        package->AddAssetObject(newAsset);
    }

    m_asset = std::move(newAsset);

    if (IsInitCalled())
    {
        CreateGpuBuffers();
    }
}

BLASRef Mesh::BuildBLAS(const Handle<Material>& material) const
{
    if (!m_asset)
    {
        HYP_LOG(Mesh, Error, "Mesh asset is not set, cannot build BLAS");

        return nullptr;
    }

    ResourceHandle resourceHandle(*m_asset->GetResource());

    Array<PackedVertex> packedVertices = m_asset->GetMeshData()->BuildPackedVertices();
    Array<uint32> packedIndices = m_asset->GetMeshData()->BuildPackedIndices();

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

bool Mesh::BuildBVH(int maxDepth)
{
    if (!m_asset)
    {
        return false;
    }

    ResourceHandle resourceHandle(*m_asset->GetResource());

    const MeshData& meshData = *m_asset->GetMeshData();

    return meshData.BuildBVH(m_bvh, maxDepth);
}

#pragma endregion Mesh

} // namespace hyperion