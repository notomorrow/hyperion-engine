/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/rt/VulkanAccelerationStructure.hpp>
#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/Material.hpp>
#include <rendering/Shared.hpp>

#include <core/utilities/Range.hpp>
#include <core/math/MathUtil.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

static VkTransformMatrixKHR ToVkTransform(const Matrix4& matrix)
{
    VkTransformMatrixKHR transform;
    std::memcpy(&transform, matrix.values, sizeof(VkTransformMatrixKHR));

    return transform;
}

static VkAccelerationStructureTypeKHR ToVkAccelerationStructureType(AccelerationStructureType type)
{
    switch (type)
    {
    case AccelerationStructureType::BOTTOM_LEVEL:
        return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    case AccelerationStructureType::TOP_LEVEL:
        return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    default:
        return VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
    }
}

#pragma region VulkanAccelerationGeometry

VulkanAccelerationGeometry::VulkanAccelerationGeometry(const GpuBufferRef& packedVerticesBuffer, const GpuBufferRef& packedIndicesBuffer, const Handle<Material>& material)
    : m_isCreated(false),
      m_packedVerticesBuffer(packedVerticesBuffer),
      m_packedIndicesBuffer(packedIndicesBuffer),
      m_material(material),
      m_geometry {}
{
}

VulkanAccelerationGeometry::~VulkanAccelerationGeometry()
{
}

bool VulkanAccelerationGeometry::IsCreated() const
{
    return m_isCreated;
}

RendererResult VulkanAccelerationGeometry::Create()
{
    if (m_isCreated)
    {
        return {};
    }

    if (!GetRenderBackend()->GetDevice()->GetFeatures().IsRaytracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support raytracing");
    }

    if (!m_packedVerticesBuffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed vertices buffer is not valid");
    }

    if (!m_packedVerticesBuffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed vertices buffer is not created");
    }

    if (!m_packedIndicesBuffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed indices buffer is not valid");
    }

    if (!m_packedIndicesBuffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed indices buffer is not created");
    }

    VkDeviceOrHostAddressConstKHR verticesAddress {
        .deviceAddress = VULKAN_CAST(m_packedVerticesBuffer)->GetBufferDeviceAddress()
    };

    VkDeviceOrHostAddressConstKHR indicesAddress {
        .deviceAddress = VULKAN_CAST(m_packedIndicesBuffer)->GetBufferDeviceAddress()
    };

    m_geometry = VkAccelerationStructureGeometryKHR { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    m_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    m_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    m_geometry.geometry = {
        .triangles = VkAccelerationStructureGeometryTrianglesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = verticesAddress,
            .vertexStride = sizeof(PackedVertex),
            .maxVertex = uint32(m_packedVerticesBuffer->Size() / sizeof(PackedVertex)),
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = indicesAddress,
            .transformData = { {} } }
    };

    m_isCreated = true;
    
    return RendererResult();
}

RendererResult VulkanAccelerationGeometry::Destroy()
{
    RendererResult result;

    SafeRelease(std::move(m_packedVerticesBuffer));
    SafeRelease(std::move(m_packedIndicesBuffer));

    m_isCreated = false;

    return result;
}

#pragma endregion VulkanAccelerationGeometry

#pragma region AccelerationStructure

VulkanAccelerationStructureBase::VulkanAccelerationStructureBase(const Matrix4& transform)
    : m_transform(transform),
      m_accelerationStructure(VK_NULL_HANDLE),
      m_deviceAddress(0),
      m_flags(ACCELERATION_STRUCTURE_FLAGS_NONE)
{
}

VulkanAccelerationStructureBase::~VulkanAccelerationStructureBase()
{
    HYP_GFX_ASSERT(
        m_accelerationStructure == VK_NULL_HANDLE,
        "Expected acceleration structure to have been destroyed before destructor call");

    HYP_GFX_ASSERT(
        m_buffer == nullptr,
        "Acceleration structure buffer should have been destroyed before destructor call");

    HYP_GFX_ASSERT(
        m_scratchBuffer == nullptr,
        "Scratch buffer should have been destroyed before destructor call");
}

RendererResult VulkanAccelerationStructureBase::CreateAccelerationStructure(
    AccelerationStructureType type,
    Span<const VkAccelerationStructureGeometryKHR> geometries,
    Span<const uint32> primitiveCounts,
    bool update,
    RTUpdateStateFlags& outUpdateStateFlags)
{
    if (update)
    {
        HYP_GFX_ASSERT(m_accelerationStructure != VK_NULL_HANDLE);
    }
    else
    {
        HYP_GFX_ASSERT(m_accelerationStructure == VK_NULL_HANDLE);
    }

    if (!GetRenderBackend()->GetDevice()->GetFeatures().IsRaytracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support raytracing");
    }

    if (!geometries)
    {
        return HYP_MAKE_ERROR(RendererError, "Geometries empty");
    }

    VkAccelerationStructureBuildGeometryInfoKHR geometryInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    geometryInfo.type = ToVkAccelerationStructureType(type);
    geometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    geometryInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometryInfo.geometryCount = uint32(geometries.Size());
    geometryInfo.pGeometries = geometries.Data();

    HYP_GFX_ASSERT(primitiveCounts.Size() == geometries.Size());

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    g_vulkanDynamicFunctions->vkGetAccelerationStructureBuildSizesKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &geometryInfo,
        primitiveCounts.Data(),
        &buildSizesInfo);

    const SizeType scratchBufferAlignment = GetRenderBackend()->GetDevice()->GetFeatures().GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
    SizeType accelerationStructureSize = MathUtil::NextMultiple(buildSizesInfo.accelerationStructureSize, 256ull);
    SizeType buildScratchSize = MathUtil::NextMultiple(buildSizesInfo.buildScratchSize, scratchBufferAlignment);
    SizeType updateScratchSize = MathUtil::NextMultiple(buildSizesInfo.updateScratchSize, scratchBufferAlignment);

    bool wasRebuilt = false;

    if (!m_buffer)
    {
        m_buffer = GetRenderBackend()->MakeGpuBuffer(GpuBufferType::ACCELERATION_STRUCTURE_BUFFER, accelerationStructureSize);
        m_buffer->SetDebugName(NAME("ASBuffer"));
        HYP_GFX_CHECK(m_buffer->Create());
    }

    HYP_GFX_CHECK(m_buffer->EnsureCapacity(accelerationStructureSize, &wasRebuilt));

    // set to true to force recreate (for debug)
    wasRebuilt = true;

    if (wasRebuilt)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE;

        if (update)
        {
            // delete the current acceleration structure once the frame is done, rather than stalling the gpu here
            GetRenderBackend()->GetCurrentFrame()->OnFrameEnd
                .Bind([oldAccelerationStructure = m_accelerationStructure](...)
                {
                    g_vulkanDynamicFunctions->vkDestroyAccelerationStructureKHR(
                        GetRenderBackend()->GetDevice()->GetDevice(),
                        oldAccelerationStructure,
                        nullptr);
                })
                .Detach();

            m_accelerationStructure = VK_NULL_HANDLE;

            // fetch the corrected acceleration structure and scratch buffer sizes
            // update was true but we need to rebuild from scratch, have to unset the UPDATE flag.
            geometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

            g_vulkanDynamicFunctions->vkGetAccelerationStructureBuildSizesKHR(
                GetRenderBackend()->GetDevice()->GetDevice(),
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &geometryInfo,
                primitiveCounts.Data(),
                &buildSizesInfo);

            accelerationStructureSize = MathUtil::NextMultiple(buildSizesInfo.accelerationStructureSize, 256ull);
            buildScratchSize = MathUtil::NextMultiple(buildSizesInfo.buildScratchSize, scratchBufferAlignment);
            updateScratchSize = MathUtil::NextMultiple(buildSizesInfo.updateScratchSize, scratchBufferAlignment);

            HYP_GFX_ASSERT(m_buffer->Size() >= accelerationStructureSize);
        }

        // to be sure it's zeroed out
        m_buffer->Memset(accelerationStructureSize, 0);

        VkAccelerationStructureCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        createInfo.buffer = VULKAN_CAST(m_buffer)->GetVulkanHandle();
        createInfo.size = accelerationStructureSize;
        createInfo.type = ToVkAccelerationStructureType(type);

        VULKAN_CHECK(g_vulkanDynamicFunctions->vkCreateAccelerationStructureKHR(
            GetRenderBackend()->GetDevice()->GetDevice(),
            &createInfo,
            VK_NULL_HANDLE,
            &m_accelerationStructure));
    }

    HYP_GFX_ASSERT(m_accelerationStructure != VK_NULL_HANDLE);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    addressInfo.accelerationStructure = m_accelerationStructure;

    m_deviceAddress = g_vulkanDynamicFunctions->vkGetAccelerationStructureDeviceAddressKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        &addressInfo);

    const SizeType scratchSize = (update && !wasRebuilt) ? updateScratchSize : buildScratchSize;

    if (m_scratchBuffer == nullptr)
    {
        m_scratchBuffer = GetRenderBackend()->MakeGpuBuffer(GpuBufferType::SCRATCH_BUFFER, scratchSize, scratchBufferAlignment);
        m_scratchBuffer->SetDebugName(NAME("ASScratchBuffer"));

        HYP_GFX_CHECK(m_scratchBuffer->Create());
    }
    else
    {
        HYP_GFX_CHECK(m_scratchBuffer->EnsureCapacity(scratchSize, scratchBufferAlignment));
    }

    // zero out scratch buffer
    m_scratchBuffer->Memset(m_scratchBuffer->Size(), 0);

    geometryInfo.dstAccelerationStructure = m_accelerationStructure;
    geometryInfo.srcAccelerationStructure = (update && !wasRebuilt) ? m_accelerationStructure : VK_NULL_HANDLE;
    geometryInfo.scratchData = { .deviceAddress = VULKAN_CAST(m_scratchBuffer)->GetBufferDeviceAddress() };

    Array<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos;
    rangeInfos.Resize(geometries.Size());

    Array<VkAccelerationStructureBuildRangeInfoKHR*> rangeInfoPtrs;
    rangeInfoPtrs.Resize(geometries.Size());

    for (SizeType i = 0; i < geometries.Size(); i++)
    {
        rangeInfos[i] = VkAccelerationStructureBuildRangeInfoKHR {
            .primitiveCount = primitiveCounts[i],
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };

        rangeInfoPtrs[i] = &rangeInfos[i];
    }

    VulkanFenceRef fence = MakeRenderObject<VulkanFence>();
    HYP_GFX_CHECK(fence->Create());
    HYP_GFX_CHECK(fence->Reset());

    VulkanCommandBufferRef commandBuffer = MakeRenderObject<VulkanCommandBuffer>(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    HYP_GFX_CHECK(commandBuffer->Create(GetRenderBackend()->GetDevice()->GetGraphicsQueue().commandPools[0]));

    HYP_GFX_CHECK(commandBuffer->Begin());

    g_vulkanDynamicFunctions->vkCmdBuildAccelerationStructuresKHR(
        commandBuffer->GetVulkanHandle(),
        uint32(rangeInfoPtrs.Size()),
        &geometryInfo,
        rangeInfoPtrs.Data());

    HYP_GFX_CHECK(commandBuffer->End());

    HYP_GFX_CHECK(commandBuffer->SubmitPrimary(&GetRenderBackend()->GetDevice()->GetGraphicsQueue(), fence, nullptr));

    SafeRelease(std::move(commandBuffer));
    SafeRelease(std::move(fence));

    ClearFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
    
    return RendererResult();
}

RendererResult VulkanAccelerationStructureBase::Destroy()
{
    RendererResult result;

    SafeRelease(std::move(m_geometries));
    SafeRelease(std::move(m_buffer));
    SafeRelease(std::move(m_scratchBuffer));

    if (m_accelerationStructure != VK_NULL_HANDLE)
    {
        g_vulkanDynamicFunctions->vkDestroyAccelerationStructureKHR(
            GetRenderBackend()->GetDevice()->GetDevice(),
            m_accelerationStructure,
            VK_NULL_HANDLE);

        m_accelerationStructure = VK_NULL_HANDLE;
    }

    HYP_GFX_ASSERT(m_buffer == nullptr);
    HYP_GFX_ASSERT(m_scratchBuffer == nullptr);

    return result;
}

void VulkanAccelerationStructureBase::RemoveGeometry(uint32 index)
{
    const auto it = m_geometries.Begin() + index;

    if (it >= m_geometries.End())
    {
        return;
    }

    SafeRelease(std::move(*it));

    m_geometries.Erase(it);

    SetNeedsRebuildFlag();
}

void VulkanAccelerationStructureBase::RemoveGeometry(const VulkanAccelerationGeometryRef& geometry)
{
    if (geometry == nullptr)
    {
        return;
    }

    const auto it = m_geometries.Find(geometry);

    if (it == m_geometries.End())
    {
        return;
    }

    SafeRelease(std::move(*it));

    m_geometries.Erase(it);

    SetNeedsRebuildFlag();
}

#pragma endregion AccelerationStructure

#pragma region TLAS

VulkanTLAS::VulkanTLAS()
    : VulkanAccelerationStructureBase()
{
}

VulkanTLAS::~VulkanTLAS()
{
    HYP_GFX_ASSERT(
        m_instancesBuffer == nullptr,
        "Instances buffer should have been destroyed before destructor call");

    HYP_GFX_ASSERT(m_accelerationStructure == VK_NULL_HANDLE, "Acceleration structure should have been destroyed before destructor call");
}

HYP_API bool VulkanTLAS::IsCreated() const
{
    return m_accelerationStructure != VK_NULL_HANDLE;
}

Array<VkAccelerationStructureGeometryKHR> VulkanTLAS::GetGeometries() const
{
    return {
        { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {
                .instances = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                    .arrayOfPointers = VK_FALSE,
                    .data = { .deviceAddress = VULKAN_CAST(m_instancesBuffer)->GetBufferDeviceAddress() } } },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR }
    };
}

Array<uint32> VulkanTLAS::GetPrimitiveCounts() const
{
    return { uint32(m_blas.Size()) };
}

RendererResult VulkanTLAS::Create()
{
    if (IsCreated())
    {
        return {};
    }

    HYP_GFX_ASSERT(m_accelerationStructure == VK_NULL_HANDLE);

    if (m_blas.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Top level acceleration structure must have at least one BLAS");
    }

    for (VulkanBLASRef& blas : m_blas)
    {
        HYP_GFX_ASSERT(blas.IsValid());

        HYP_GFX_CHECK(blas->Create());
    }

    HYP_GFX_CHECK(BuildInstancesBuffer());

    RTUpdateStateFlags updateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    HYP_GFX_CHECK(CreateAccelerationStructure(GetType(), GetGeometries(), GetPrimitiveCounts(), false, updateStateFlags));

    HYP_GFX_ASSERT(updateStateFlags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    HYP_GFX_CHECK(BuildMeshDescriptionsBuffer());
    updateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;
    
    return RendererResult();
}

RendererResult VulkanTLAS::Destroy()
{
    SafeRelease(std::move(m_instancesBuffer));
    SafeRelease(std::move(m_meshDescriptionsBuffer));
    SafeRelease(std::move(m_scratchBuffer));
    SafeRelease(std::move(m_blas));

    return VulkanAccelerationStructureBase::Destroy();
}

void VulkanTLAS::AddBLAS(const BLASRef& blas)
{
    HYP_GFX_ASSERT(blas != nullptr);

    if (m_blas.FindAs(blas) != m_blas.End())
    {
        return;
    }

    VulkanBLASRef vulkanBlas { blas };

    HYP_GFX_ASSERT(vulkanBlas->IsCreated());
    HYP_GFX_ASSERT(!vulkanBlas->GetGeometries().Empty());

    for (const VulkanAccelerationGeometryRef& geometry : vulkanBlas->GetGeometries())
    {
        HYP_GFX_ASSERT(geometry != nullptr);
        HYP_GFX_ASSERT(geometry->GetPackedVerticesBuffer() != nullptr);
        HYP_GFX_ASSERT(geometry->GetPackedIndicesBuffer() != nullptr);
    }

    /*if (IsCreated())
    {
        // If the TLAS is already created, we need to ensure that the BLAS is created as well.
        if (!vulkanBlas->IsCreated())
        {
            HYP_GFX_ASSERT(vulkanBlas->Create());
        }
    }*/

    m_blas.PushBack(std::move(vulkanBlas));

    SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
}

void VulkanTLAS::RemoveBLAS(const BLASRef& blas)
{
    auto it = m_blas.FindAs(blas);

    if (it != m_blas.End())
    {
        VulkanBLASRef& vulkanBlas = *it;
        m_blas.Erase(it);

        SafeRelease(std::move(vulkanBlas));

        SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
    }
}

bool VulkanTLAS::HasBLAS(const BLASRef& blas)
{
    if (!blas.IsValid())
    {
        return false;
    }

    return m_blas.Contains(blas);
}

RendererResult VulkanTLAS::BuildInstancesBuffer()
{
    return BuildInstancesBuffer(0, uint32(m_blas.Size()));
}

RendererResult VulkanTLAS::BuildInstancesBuffer(uint32 first, uint32 last)
{
    if (last <= first)
    {
        // nothing to update
        return RendererResult();
    }

    last = MathUtil::Min(m_blas.Size(), last);

    constexpr SizeType minInstancesBufferSize = sizeof(VkAccelerationStructureInstanceKHR);
    const SizeType instancesBufferSize = MathUtil::Max(minInstancesBufferSize, m_blas.Size() * sizeof(VkAccelerationStructureInstanceKHR));

    bool instancesBufferRecreated = false;

    if (!m_instancesBuffer)
    {
        m_instancesBuffer = GetRenderBackend()->MakeGpuBuffer(GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER, instancesBufferSize);
        m_instancesBuffer->SetDebugName(NAME("ASInstancesBuffer"));
        HYP_GFX_CHECK(m_instancesBuffer->Create());

        instancesBufferRecreated = true;
    }
    else
    {
        m_instancesBuffer->EnsureCapacity(instancesBufferSize, &instancesBufferRecreated);
    }

    if (instancesBufferRecreated)
    {
        // zero out buffer
        m_instancesBuffer->Memset(m_instancesBuffer->Size(), 0x0);

        // set dirty range to all elements if resized or newly created
        first = 0;
        last = uint32(m_blas.Size());
    }

    if (m_blas.Empty() || last <= first)
    {
        // no need to update the data inside
        return RendererResult();
    }

    Array<VkAccelerationStructureInstanceKHR> instances;
    instances.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        const VulkanBLASRef& blas = m_blas[i];
        HYP_GFX_ASSERT(blas != nullptr);

        const uint32 instanceIndex = i; /* Index of mesh in mesh descriptions buffer. */

        instances[i - first] = VkAccelerationStructureInstanceKHR {
            .transform = ToVkTransform(blas->GetTransform()),
            .instanceCustomIndex = instanceIndex,
            .mask = 0xff,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blas->GetDeviceAddress()
        };
    }

    HYP_GFX_ASSERT(m_instancesBuffer != nullptr);
    HYP_GFX_ASSERT(m_instancesBuffer->Size() >= (first + instances.Size()) * sizeof(VkAccelerationStructureInstanceKHR));

    m_instancesBuffer->Copy(
        first * sizeof(VkAccelerationStructureInstanceKHR),
        instances.Size() * sizeof(VkAccelerationStructureInstanceKHR),
        instances.Data());
    
    return RendererResult();
}

RendererResult VulkanTLAS::BuildMeshDescriptionsBuffer()
{
    return BuildMeshDescriptionsBuffer(0u, uint32(m_blas.Size()));
}

RendererResult VulkanTLAS::BuildMeshDescriptionsBuffer(uint32 first, uint32 last)
{
    if (last <= first)
    {
        // nothing to update
        return RendererResult();
    }

    last = MathUtil::Min(m_blas.Size(), last);

    constexpr SizeType minMeshDescriptionsBufferSize = sizeof(MeshDescription);
    const SizeType meshDescriptionsBufferSize = MathUtil::Max(minMeshDescriptionsBufferSize, sizeof(MeshDescription) * m_blas.Size());

    bool meshDescriptionsBufferRecreated = false;

    if (!m_meshDescriptionsBuffer)
    {
        m_meshDescriptionsBuffer = GetRenderBackend()->MakeGpuBuffer(GpuBufferType::SSBO, meshDescriptionsBufferSize);
        m_meshDescriptionsBuffer->SetDebugName(NAME("ASMeshDescriptionsBuffer"));
        HYP_GFX_CHECK(m_meshDescriptionsBuffer->Create());

        meshDescriptionsBufferRecreated = true;
    }
    else
    {
        m_meshDescriptionsBuffer->EnsureCapacity(meshDescriptionsBufferSize, &meshDescriptionsBufferRecreated);
    }

    if (meshDescriptionsBufferRecreated)
    {
        // zero out buffer
        m_meshDescriptionsBuffer->Memset(m_meshDescriptionsBuffer->Size(), 0x0);

        // set dirty range to all elements if resized or newly created
        first = 0;
        last = uint32(m_blas.Size());
    }

    if (m_blas.Empty() || last <= first)
    {
        // no need to update the data inside
        return RendererResult();
    }

    Array<MeshDescription> meshDescriptions;
    meshDescriptions.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        const VulkanBLASRef& blas = m_blas[i];

        MeshDescription& meshDescription = meshDescriptions[i - first];
        Memory::MemSet(&meshDescription, 0, sizeof(MeshDescription));

        HYP_GFX_ASSERT(blas->GetGeometries().Any(), "No geometries added to BLAS node %u!", i);

        const Handle<Material>& material = blas->GetGeometries()[0]->GetMaterial();

        if (material.IsValid())
        {
            // /// FIXME: This needs to use new resource binding system
            // // Must be initialized (AccelerationGeometry calls IncRef() and DecRef())
            // HYP_GFX_ASSERT(material->GetRenderResource().GetBufferIndex() != ~0u);
        }

        meshDescription.vertexBufferAddress = VULKAN_CAST(blas->GetGeometries()[0]->GetPackedVerticesBuffer())->GetBufferDeviceAddress();
        meshDescription.indexBufferAddress = VULKAN_CAST(blas->GetGeometries()[0]->GetPackedIndicesBuffer())->GetBufferDeviceAddress();
        /// FIXME: This needs to use new resource binding system
        meshDescription.materialIndex = ~0u;
        // meshDescription.materialIndex = material.IsValid() ? material->GetRenderResource().GetBufferIndex() : ~0u;
        meshDescription.numIndices = uint32(blas->GetGeometries()[0]->GetPackedIndicesBuffer()->Size() / sizeof(uint32));
        meshDescription.numVertices = uint32(blas->GetGeometries()[0]->GetPackedVerticesBuffer()->Size() / sizeof(PackedVertex));
    }

    HYP_GFX_ASSERT(m_meshDescriptionsBuffer != nullptr);
    HYP_GFX_ASSERT(m_meshDescriptionsBuffer->Size() >= (first + meshDescriptions.Size()) * sizeof(MeshDescription));

    m_meshDescriptionsBuffer->Copy(
        first * sizeof(MeshDescription),
        meshDescriptions.Size() * sizeof(MeshDescription),
        meshDescriptions.Data());
    
    return RendererResult();
}

RendererResult VulkanTLAS::UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags)
{
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING)
    {
        return Rebuild(outUpdateStateFlags);
    }

    Range<uint32> dirtyRange {};

    for (uint32 i = 0; i < uint32(m_blas.Size()); i++)
    {
        const VulkanBLASRef& blas = m_blas[i];
        HYP_GFX_ASSERT(blas != nullptr);

        RTUpdateStateFlags blasUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;
        HYP_GFX_CHECK(blas->UpdateStructure(blasUpdateStateFlags));

        if (blasUpdateStateFlags)
        {
            dirtyRange |= Range { i, i + 1 };
        }
    }

    if (dirtyRange)
    {
        HYP_GFX_CHECK(BuildInstancesBuffer(dirtyRange.GetStart(), dirtyRange.GetEnd()));
        HYP_GFX_CHECK(BuildMeshDescriptionsBuffer(dirtyRange.GetStart(), dirtyRange.GetEnd()));

        HYP_GFX_CHECK(CreateAccelerationStructure(GetType(), GetGeometries(), GetPrimitiveCounts(), true, outUpdateStateFlags));

        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS | RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES;
    }
    
    return RendererResult();
}

RendererResult VulkanTLAS::Rebuild(RTUpdateStateFlags& outUpdateStateFlags)
{
    HYP_GFX_ASSERT(m_accelerationStructure != VK_NULL_HANDLE);

    // check each BLAS, assert that it is valid.
    for (const VulkanBLASRef& blas : m_blas)
    {
        HYP_GFX_ASSERT(blas != nullptr);
        HYP_GFX_ASSERT(blas->IsCreated());
        HYP_GFX_ASSERT(!blas->GetGeometries().Empty());

        for (const VulkanAccelerationGeometryRef& geometry : blas->GetGeometries())
        {
            HYP_GFX_ASSERT(geometry != nullptr);
            HYP_GFX_ASSERT(geometry->GetPackedVerticesBuffer() != nullptr);
            HYP_GFX_ASSERT(geometry->GetPackedIndicesBuffer() != nullptr);
        }
    }

    HYP_GFX_CHECK(BuildInstancesBuffer());
    outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES;

    HYP_GFX_CHECK(CreateAccelerationStructure(
        GetType(),
        GetGeometries(),
        GetPrimitiveCounts(),
        true,
        outUpdateStateFlags));

    HYP_GFX_CHECK(BuildMeshDescriptionsBuffer());
    outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;
    
    return RendererResult();
}

#pragma endregion TLAS

#pragma region BLAS

VulkanBLAS::VulkanBLAS(
    const GpuBufferRef& packedVerticesBuffer,
    const GpuBufferRef& packedIndicesBuffer,
    const Handle<Material>& material,
    const Matrix4& transform)
    : VulkanAccelerationStructureBase(transform),
      m_packedVerticesBuffer(packedVerticesBuffer),
      m_packedIndicesBuffer(packedIndicesBuffer),
      m_material(material)
{
    m_geometries.PushBack(MakeRenderObject<VulkanAccelerationGeometry>(
        m_packedVerticesBuffer,
        m_packedIndicesBuffer,
        m_material));
}

VulkanBLAS::~VulkanBLAS() = default;

HYP_API bool VulkanBLAS::IsCreated() const
{
    return m_accelerationStructure != VK_NULL_HANDLE;
}

RendererResult VulkanBLAS::Create()
{
    if (IsCreated())
    {
        return {};
    }

    RendererResult result;

    Array<VkAccelerationStructureGeometryKHR> geometries(m_geometries.Size());
    Array<uint32> primitiveCounts(m_geometries.Size());

    if (m_geometries.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create BLAS with zero geometries");
    }

    for (SizeType i = 0; i < m_geometries.Size(); i++)
    {
        const VulkanAccelerationGeometryRef& geometry = m_geometries[i];

        if (!geometry->IsCreated())
        {
            HYP_GFX_CHECK(geometry->Create());
        }

        geometries[i] = geometry->m_geometry;
        primitiveCounts[i] = uint32(geometry->GetPackedIndicesBuffer()->Size() / sizeof(uint32) / 3);

        if (primitiveCounts[i] == 0)
        {
            return HYP_MAKE_ERROR(RendererError, "Cannot create BLAS -- geometry has zero indices");
        }
    }

    RTUpdateStateFlags updateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    HYP_GFX_CHECK(CreateAccelerationStructure(GetType(), geometries, primitiveCounts, false, updateStateFlags));
    HYP_GFX_ASSERT(updateStateFlags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    return RendererResult();
}

RendererResult VulkanBLAS::Destroy()
{
    SafeRelease(std::move(m_packedVerticesBuffer));
    SafeRelease(std::move(m_packedIndicesBuffer));

    return VulkanAccelerationStructureBase::Destroy();
}

RendererResult VulkanBLAS::UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags)
{
    outUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MATERIAL;

        ClearFlag(ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
    }

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_TRANSFORM;

        ClearFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);
    }

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING)
    {
        return Rebuild(outUpdateStateFlags);
    }
    
    return RendererResult();
}

RendererResult VulkanBLAS::Rebuild(RTUpdateStateFlags& outUpdateStateFlags)
{
    Array<VkAccelerationStructureGeometryKHR> geometries(m_geometries.Size());
    Array<uint32> primitiveCounts(m_geometries.Size());

    for (SizeType i = 0; i < m_geometries.Size(); i++)
    {
        const VulkanAccelerationGeometryRef& geometry = m_geometries[i];
        HYP_GFX_ASSERT(geometry != nullptr);

        geometries[i] = geometry->m_geometry;
        primitiveCounts[i] = uint32(geometry->GetPackedIndicesBuffer()->Size() / sizeof(uint32) / 3);
    }

    HYP_GFX_CHECK(CreateAccelerationStructure(GetType(), geometries, primitiveCounts, true, outUpdateStateFlags));

    m_flags &= ~(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING | ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);

    return RendererResult();
}

#pragma endregion BLAS

} // namespace hyperion
