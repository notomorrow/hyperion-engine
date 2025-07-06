/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_RENDERER_VULKAN_ACCELERATION_STRUCTURE_HPP
#define HYPERION_RENDERER_BACKEND_RENDERER_VULKAN_ACCELERATION_STRUCTURE_HPP

#include <core/math/Matrix4.hpp>

#include <core/containers/Array.hpp>

#include <core/utilities/Span.hpp>

#include <core/Handle.hpp>

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <streaming/StreamedMeshData.hpp>

#include <Types.hpp>

namespace hyperion {

class Entity;
class Material;

class VulkanAccelerationGeometry final : public RenderObject<VulkanAccelerationGeometry>
{
public:
    friend class VulkanTLAS;
    friend class VulkanBLAS;

    HYP_API VulkanAccelerationGeometry(
        const VulkanGpuBufferRef& packedVerticesBuffer,
        const VulkanGpuBufferRef& packedIndicesBuffer,
        const Handle<Material>& material);

    HYP_API virtual ~VulkanAccelerationGeometry() override;

    HYP_FORCE_INLINE const VulkanGpuBufferRef& GetPackedVerticesBuffer() const
    {
        return m_packedVerticesBuffer;
    }

    HYP_FORCE_INLINE const VulkanGpuBufferRef& GetPackedIndicesBuffer() const
    {
        return m_packedIndicesBuffer;
    }

    HYP_FORCE_INLINE const Handle<Material>& GetMaterial() const
    {
        return m_material;
    }

    HYP_API bool IsCreated() const;

    HYP_API RendererResult Create();
    /* Remove from the parent acceleration structure */
    HYP_API RendererResult Destroy();

private:
    bool m_isCreated;

    VulkanGpuBufferRef m_packedVerticesBuffer;
    VulkanGpuBufferRef m_packedIndicesBuffer;

    Handle<Material> m_material;

    VkAccelerationStructureGeometryKHR m_geometry;
};

using VulkanAccelerationGeometryRef = RenderObjectHandle_Strong<VulkanAccelerationGeometry>;
using VulkanAccelerationGeometryWeakRef = RenderObjectHandle_Weak<VulkanAccelerationGeometry>;

class VulkanAccelerationStructureBase
{
public:
    HYP_API VulkanAccelerationStructureBase(const Matrix4& transform = Matrix4::Identity());
    HYP_API virtual ~VulkanAccelerationStructureBase();

    HYP_FORCE_INLINE const VulkanGpuBufferRef& GetBuffer() const
    {
        return m_buffer;
    }

    HYP_FORCE_INLINE const VkAccelerationStructureKHR& GetVulkanHandle() const
    {
        return m_accelerationStructure;
    }

    HYP_FORCE_INLINE uint64 GetDeviceAddress() const
    {
        return m_deviceAddress;
    }

    HYP_FORCE_INLINE AccelerationStructureFlags GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE void SetFlag(AccelerationStructureFlagBits flag)
    {
        m_flags = AccelerationStructureFlags(m_flags | flag);
    }

    HYP_FORCE_INLINE void ClearFlag(AccelerationStructureFlagBits flag)
    {
        m_flags = AccelerationStructureFlags(m_flags & ~flag);
    }

    HYP_FORCE_INLINE const Array<VulkanAccelerationGeometryRef>& GetGeometries() const
    {
        return m_geometries;
    }

    HYP_FORCE_INLINE void AddGeometry(const VulkanAccelerationGeometryRef& geometry)
    {
        if (!geometry || m_geometries.Contains(geometry))
        {
            return;
        }

        m_geometries.PushBack(geometry);
        SetNeedsRebuildFlag();
    }

    HYP_API void RemoveGeometry(uint32 index);

    /*! \brief Remove the geometry from the internal list of Nodes and set a flag that the
     * structure needs to be rebuilt. Will not automatically rebuild.
     */
    HYP_API void RemoveGeometry(const VulkanAccelerationGeometryRef& geometry);

    HYP_FORCE_INLINE const Matrix4& GetTransform() const
    {
        return m_transform;
    }

    HYP_FORCE_INLINE void SetTransform(const Matrix4& transform)
    {
        if (m_transform == transform)
        {
            // same transforms, don't set the flag
            return;
        }

        m_transform = transform;
        SetTransformUpdateFlag();
    }

    HYP_API RendererResult Destroy();

protected:
    HYP_FORCE_INLINE void SetTransformUpdateFlag()
    {
        SetFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);
    }

    HYP_FORCE_INLINE void SetNeedsRebuildFlag()
    {
        SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
    }

    RendererResult CreateAccelerationStructure(
        AccelerationStructureType type,
        Span<const VkAccelerationStructureGeometryKHR> geometries,
        Span<const uint32> primitiveCounts,
        bool update,
        RTUpdateStateFlags& outUpdateStateFlags);
    
    VulkanGpuBufferRef m_buffer;
    VulkanGpuBufferRef m_scratchBuffer;
    Array<VulkanAccelerationGeometryRef> m_geometries;
    Matrix4 m_transform;
    VkAccelerationStructureKHR m_accelerationStructure;
    uint64 m_deviceAddress;
    AccelerationStructureFlags m_flags;
};

class VulkanBLAS final : public BLASBase, public VulkanAccelerationStructureBase
{
public:
    HYP_API VulkanBLAS(
        const VulkanGpuBufferRef& packedVerticesBuffer,
        const VulkanGpuBufferRef& packedIndicesBuffer,
        const Handle<Material>& material,
        const Matrix4& transform);
    HYP_API virtual ~VulkanBLAS() override;

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual void SetTransform(const Matrix4& transform) override
    {
        VulkanAccelerationStructureBase::SetTransform(transform);
    }

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    HYP_API RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags);

private:
    RendererResult Rebuild(RTUpdateStateFlags& outUpdateStateFlags);

    VulkanGpuBufferRef m_packedVerticesBuffer;
    VulkanGpuBufferRef m_packedIndicesBuffer;
    Handle<Material> m_material;
};

class VulkanTLAS : public TLASBase, public VulkanAccelerationStructureBase
{
public:
    HYP_API VulkanTLAS();
    HYP_API virtual ~VulkanTLAS() override;

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual void AddBLAS(const BLASRef& blas) override;
    HYP_API virtual void RemoveBLAS(const BLASRef& blas) override;
    HYP_API virtual bool HasBLAS(const BLASRef& blas) override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    HYP_API virtual RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags) override;

private:
    RendererResult Rebuild(RTUpdateStateFlags& outUpdateStateFlags);

    Array<VkAccelerationStructureGeometryKHR> GetGeometries() const;
    Array<uint32> GetPrimitiveCounts() const;

    RendererResult BuildInstancesBuffer();
    RendererResult BuildInstancesBuffer(uint32 first, uint32 last);

    RendererResult BuildMeshDescriptionsBuffer();
    RendererResult BuildMeshDescriptionsBuffer(uint32 first, uint32 last);

    Array<VulkanBLASRef> m_blas;
    VulkanGpuBufferRef m_instancesBuffer;
};

} // namespace hyperion

#endif
