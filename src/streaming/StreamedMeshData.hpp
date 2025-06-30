/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMED_MESH_DATA_HPP
#define HYPERION_STREAMED_MESH_DATA_HPP

#include <streaming/StreamedData.hpp>

#include <core/math/Vertex.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/Array.hpp>
#include <core/utilities/Optional.hpp>

#include <core/object/HypObject.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_STRUCT()

struct MeshData
{
    HYP_FIELD()
    Array<Vertex> vertices;

    HYP_FIELD()
    Array<uint32> indices;
    MeshData() = default;

    MeshData(Array<Vertex>&& vertices, Array<uint32>&& indices)
        : vertices(std::move(vertices)),
          indices(std::move(indices))
    {
        // DEBUGGING
        AssertThrowMsg(this->vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");
    }

    MeshData(const Array<Vertex>& vertices, const Array<uint32>& indices)
        : vertices(vertices),
          indices(indices)
    {
        // DEBUGGING
        AssertThrowMsg(this->vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");
    }

    MeshData(const MeshData& other)
        : vertices(other.vertices),
          indices(other.indices)
    {
        // DEBUGGING
        AssertThrowMsg(vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");
    }

    MeshData(MeshData&& other) noexcept
        : vertices(std::move(other.vertices)),
          indices(std::move(other.indices))
    {
        // DEBUGGING
        AssertThrowMsg(this->vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");
    }

    MeshData& operator=(const MeshData& other)
    {
        if (this != &other)
        {
            vertices = other.vertices;
            indices = other.indices;
        }
        // DEBUGGING
        AssertThrowMsg(vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");

        return *this;
    }

    MeshData& operator=(MeshData&& other) noexcept
    {
        if (this != &other)
        {
            vertices = std::move(other.vertices);
            indices = std::move(other.indices);
        }
        // DEBUGGING
        AssertThrowMsg(vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");

        return *this;
    }

    ~MeshData()
    {
        // DEBUGGING
        AssertThrowMsg(vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        // @FIXME: Find a better way to hash it without needing to hash the entire mesh data

        HashCode hc;
        hc.Add(vertices.GetHashCode());
        hc.Add(indices.GetHashCode());

        return hc;
    }
};

class HYP_API StreamedMeshData final : public StreamedDataBase
{
    StreamedMeshData(StreamedDataState initialState, MeshData&& meshData, ResourceHandle& outResourceHandle);

public:
    StreamedMeshData();
    StreamedMeshData(const MeshData& meshData, ResourceHandle& outResourceHandle);
    StreamedMeshData(MeshData&& meshData, ResourceHandle& outResourceHandle);

    StreamedMeshData(const StreamedMeshData& other) = delete;
    StreamedMeshData& operator=(const StreamedMeshData& other) = delete;
    StreamedMeshData(StreamedMeshData&& other) noexcept = delete;
    StreamedMeshData& operator=(StreamedMeshData&& other) noexcept = delete;
    virtual ~StreamedMeshData() override;

    const MeshData& GetMeshData() const;

    HYP_FORCE_INLINE SizeType NumVertices() const
    {
        return m_numVertices;
    }

    HYP_FORCE_INLINE SizeType NumIndices() const
    {
        return m_numIndices;
    }

    virtual HashCode GetDataHashCode() const override
    {
        return m_streamedData ? m_streamedData->GetDataHashCode() : HashCode(0);
    }

protected:
    virtual bool IsInMemory_Internal() const override;

    virtual void Load_Internal() const override;
    virtual void Unpage_Internal() override;

private:
    void LoadMeshData(const ByteBuffer& byteBuffer) const;

    RC<StreamedDataBase> m_streamedData;

    SizeType m_numVertices;
    SizeType m_numIndices;

    mutable Optional<MeshData> m_meshData;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace hyperion

#endif