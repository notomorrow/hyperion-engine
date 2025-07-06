/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

#include <core/utilities/Pair.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vertex.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderResource.hpp>
#include <rendering/ShaderManager.hpp>

#include <rendering/RenderStructs.hpp>
#include <rendering/RenderObject.hpp>

#include <streaming/StreamedMeshData.hpp>

#include <cstdint>

namespace hyperion {

class Mesh;
class Material;
class CmdList;

class RenderMesh final : public RenderResourceBase
{
public:
    RenderMesh(Mesh* mesh);
    RenderMesh(RenderMesh&& other) noexcept;
    virtual ~RenderMesh() override;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const GpuBufferRef& GetVertexBuffer() const
    {
        return m_vbo;
    }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const GpuBufferRef& GetIndexBuffer() const
    {
        return m_ibo;
    }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE uint32 NumIndices() const
    {
        return m_numIndices;
    }

    void SetVertexAttributes(const VertexAttributeSet& vertexAttributes);
    void SetStreamedMeshData(const RC<StreamedMeshData>& streamedMeshData);

    void Render(CmdList& cmd, uint32 numInstances = 1, uint32 instanceIndex = 0) const;
    void RenderIndirect(CmdList& cmd, const GpuBufferRef& indirectBuffer, uint32 bufferOffset = 0) const;

    void PopulateIndirectDrawCommand(IndirectDrawCommand& out);

    // Build raytracing acceleration structure
    BLASRef BuildBLAS(const Handle<Material>& material) const;

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

private:
    Array<PackedVertex> BuildPackedVertices() const;
    Array<uint32> BuildPackedIndices() const;

    static Array<float> BuildVertexBuffer(const VertexAttributeSet& vertexAttributes, const MeshData& meshData);

    void UploadMeshData();

    Mesh* m_mesh;

    VertexAttributeSet m_vertexAttributes;

    RC<StreamedMeshData> m_streamedMeshData;
    mutable ResourceHandle m_streamedMeshDataHandle;

    GpuBufferRef m_vbo;
    GpuBufferRef m_ibo;

    uint32 m_numIndices;
};

} // namespace hyperion
