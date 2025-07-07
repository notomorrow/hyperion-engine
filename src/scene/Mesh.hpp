/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

#include <core/utilities/Pair.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vertex.hpp>

#include <scene/BVH.hpp>
#include <scene/RenderProxyable.hpp>

#include <rendering/RenderableAttributes.hpp>

#include <rendering/RenderStructs.hpp>
#include <rendering/RenderObject.hpp>

#include <streaming/StreamedMeshData.hpp>

#include <cstdint>

namespace hyperion {

class BVHNode;
class RenderMesh;

/*! \brief Represents a 3D mesh in the engine from the Game thread, containing vertex data, indices, and rendering attributes.
 *  \details This class is used to manage mesh data, including streamed meshes, and provides methods for manipulating mesh data at runtime. */
HYP_CLASS()
class HYP_API Mesh final : public HypObject<Mesh>
{
    HYP_OBJECT_BODY(Mesh);

public:
    using Index = uint32;

    static Pair<Array<Vertex>, Array<uint32>> CalculateIndices(const Array<Vertex>& vertices);

    Mesh();
    Mesh(RC<StreamedMeshData> streamedMeshData, Topology topology, const VertexAttributeSet& vertexAttributes);
    Mesh(RC<StreamedMeshData> streamedMeshData, Topology topology = TOP_TRIANGLES);
    Mesh(Array<Vertex> vertices, Array<uint32> indices, Topology topology, const VertexAttributeSet& vertexAttributes);
    Mesh(Array<Vertex> vertices, Array<uint32> indices, Topology topology = TOP_TRIANGLES);

    Mesh(const Mesh& other) = delete;
    Mesh& operator=(const Mesh& other) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    ~Mesh();

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetName(Name name)
    {
        m_name = name;
    }

    HYP_FORCE_INLINE RenderMesh& GetRenderResource() const
    {
        return *m_renderResource;
    }

    void SetVertices(Span<const Vertex> vertices);
    void SetVertices(Span<const Vertex> vertices, Span<const uint32> indices);

    const RC<StreamedMeshData>& GetStreamedMeshData() const;

    void SetStreamedMeshData(RC<StreamedMeshData> streamedMeshData);

    HYP_METHOD()
    uint32 NumIndices() const;

    HYP_METHOD(Property = "VertexAttributes")
    HYP_FORCE_INLINE const VertexAttributeSet& GetVertexAttributes() const
    {
        return m_meshAttributes.vertexAttributes;
    }

    HYP_METHOD(Property = "VertexAttributes")
    void SetVertexAttributes(const VertexAttributeSet& vertexAttributes);

    HYP_FORCE_INLINE const MeshAttributes& GetMeshAttributes() const
    {
        return m_meshAttributes;
    }

    void SetMeshAttributes(const MeshAttributes& attributes);

    HYP_METHOD(Property = "Topology")
    HYP_FORCE_INLINE Topology GetTopology() const
    {
        return m_meshAttributes.topology;
    }

    void CalculateNormals(bool weighted = false);
    void CalculateTangents();

    HYP_METHOD()
    void InvertNormals();

    /*! \brief Get the axis-aligned bounding box for the mesh.
        If the mesh has not been initialized, the AABB will be invalid, unless SetAABB() has been called.
        Otherwise, the AABB will be calculated from the mesh vertices. */
    HYP_METHOD(Property = "AABB", Serialize = true, Editor = true)
    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    /*! \brief Manually set the AABB for the mesh. If CalculateAABB is called after this, or the mesh data is changed, the
        manually set AABB will be overwritten. */
    HYP_METHOD(Property = "AABB", Serialize = true, Editor = true)
    HYP_FORCE_INLINE void SetAABB(const BoundingBox& aabb)
    {
        m_aabb = aabb;
    }

    /*! \brief Set the mesh to be able to have Render* methods called without needing to have its resources claimed.
     *  \note Init() must be called before this method. */
    void SetPersistentRenderResourceEnabled(bool enabled);

    HYP_FORCE_INLINE const BVHNode& GetBVH() const
    {
        return m_bvh;
    }

    HYP_METHOD()
    bool BuildBVH(int maxDepth = 3);

private:
    void Init() override;

    void CalculateAABB();

    HYP_FIELD(Serialize, Editor)
    Name m_name;

    MeshAttributes m_meshAttributes;

    // Must be before m_streamedMeshData, needs to get constructed first to use as out parameter to construct StreamedMeshData.
    mutable ResourceHandle m_streamedMeshDataResourceHandle;
    RC<StreamedMeshData> m_streamedMeshData;

    mutable BoundingBox m_aabb;

    HYP_FIELD(Serialize)
    BVHNode m_bvh;

    RenderMesh* m_renderResource;
    ResourceHandle m_renderPersistent;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace hyperion

