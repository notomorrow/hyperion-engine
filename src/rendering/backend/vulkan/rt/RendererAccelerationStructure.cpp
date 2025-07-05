/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/rt/RendererAccelerationStructure.hpp>
#include <rendering/backend/vulkan/RendererFence.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <rendering/RenderMaterial.hpp>

// @TODO: Refactor to not need this include
#include <scene/Material.hpp>

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

VulkanAccelerationGeometry::VulkanAccelerationGeometry(const VulkanGpuBufferRef& packedVerticesBuffer, const VulkanGpuBufferRef& packedIndicesBuffer, const Handle<Material>& material)
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

    if (m_material.IsValid())
    {
        m_material->GetRenderResource().IncRef();
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
        .deviceAddress = m_packedVerticesBuffer->GetBufferDeviceAddress()
    };

    VkDeviceOrHostAddressConstKHR indicesAddress {
        .deviceAddress = m_packedIndicesBuffer->GetBufferDeviceAddress()
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

    return {};
}

RendererResult VulkanAccelerationGeometry::Destroy()
{
    if (m_material.IsValid())
    {
        m_material->GetRenderResource().DecRef();
    }

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
    const std::vector<VkAccelerationStructureGeometryKHR>& geometries,
    const std::vector<uint32>& primitiveCounts,
    bool update,
    RTUpdateStateFlags& outUpdateStateFlags)
{
    if (update)
    {
        HYP_GFX_ASSERT(m_accelerationStructure != VK_NULL_HANDLE);

        // TEMP: we should have two TLASes and two RT descriptor sets I suppose...
        // HYPERION_BUBBLE_ERRORS(instance->GetDevice()->Wait());
    }
    else
    {
        HYP_GFX_ASSERT(m_accelerationStructure == VK_NULL_HANDLE);
    }

    if (!GetRenderBackend()->GetDevice()->GetFeatures().IsRaytracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support raytracing");
    }

    if (geometries.empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Geometries empty");
    }

    RendererResult result;

    VkAccelerationStructureBuildGeometryInfoKHR geometryInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    geometryInfo.type = ToVkAccelerationStructureType(type);
    geometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    geometryInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometryInfo.geometryCount = uint32(geometries.size());
    geometryInfo.pGeometries = geometries.data();

    HYP_GFX_ASSERT(primitiveCounts.size() == geometries.size());

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    GetRenderBackend()->GetDevice()->GetFeatures().dynFunctions.vkGetAccelerationStructureBuildSizesKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &geometryInfo,
        primitiveCounts.data(),
        &buildSizesInfo);

    const SizeType scratchBufferAlignment = GetRenderBackend()->GetDevice()->GetFeatures().GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
    SizeType accelerationStructureSize = MathUtil::NextMultiple(buildSizesInfo.accelerationStructureSize, 256ull);
    SizeType buildScratchSize = MathUtil::NextMultiple(buildSizesInfo.buildScratchSize, scratchBufferAlignment);
    SizeType updateScratchSize = MathUtil::NextMultiple(buildSizesInfo.updateScratchSize, scratchBufferAlignment);

    bool wasRebuilt = false;

    if (!m_buffer)
    {
        m_buffer = MakeRenderObject<VulkanGpuBuffer>(GpuBufferType::ACCELERATION_STRUCTURE_BUFFER, accelerationStructureSize);
        HYPERION_PASS_ERRORS(m_buffer->Create(), result);
    }

    HYPERION_BUBBLE_ERRORS(m_buffer->EnsureCapacity(
        accelerationStructureSize,
        &wasRebuilt));

    // force recreate (for debug)
    wasRebuilt = true;

    if (wasRebuilt)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE;

        if (update)
        {
            HYPERION_BUBBLE_ERRORS(GetRenderBackend()->GetDevice()->Wait()); // To prevent deletion while in use

            // delete the current acceleration structure
            GetRenderBackend()->GetDevice()->GetFeatures().dynFunctions.vkDestroyAccelerationStructureKHR(
                GetRenderBackend()->GetDevice()->GetDevice(),
                m_accelerationStructure,
                nullptr);

            m_accelerationStructure = VK_NULL_HANDLE;

            // fetch the corrected acceleration structure and scratch buffer sizes
            // update was true but we need to rebuild from scratch, have to unset the UPDATE flag.
            geometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

            GetRenderBackend()->GetDevice()->GetFeatures().dynFunctions.vkGetAccelerationStructureBuildSizesKHR(
                GetRenderBackend()->GetDevice()->GetDevice(),
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &geometryInfo,
                primitiveCounts.data(),
                &buildSizesInfo);

            accelerationStructureSize = MathUtil::NextMultiple(buildSizesInfo.accelerationStructureSize, 256ull);
            buildScratchSize = MathUtil::NextMultiple(buildSizesInfo.buildScratchSize, scratchBufferAlignment);
            updateScratchSize = MathUtil::NextMultiple(buildSizesInfo.updateScratchSize, scratchBufferAlignment);

            HYP_GFX_ASSERT(m_buffer->Size() >= accelerationStructureSize);
        }

        // to be sure it's zeroed out
        m_buffer->Memset(accelerationStructureSize, 0);

        VkAccelerationStructureCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        createInfo.buffer = m_buffer->GetVulkanHandle();
        createInfo.size = accelerationStructureSize;
        createInfo.type = ToVkAccelerationStructureType(type);

        HYPERION_VK_PASS_ERRORS(
            GetRenderBackend()->GetDevice()->GetFeatures().dynFunctions.vkCreateAccelerationStructureKHR(
                GetRenderBackend()->GetDevice()->GetDevice(),
                &createInfo,
                VK_NULL_HANDLE,
                &m_accelerationStructure),
            result);

        if (!result)
        {
            HYPERION_IGNORE_ERRORS(Destroy());

            return result;
        }
    }

    HYP_GFX_ASSERT(m_accelerationStructure != VK_NULL_HANDLE);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    addressInfo.accelerationStructure = m_accelerationStructure;

    m_deviceAddress = GetRenderBackend()->GetDevice()->GetFeatures().dynFunctions.vkGetAccelerationStructureDeviceAddressKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        &addressInfo);

    const SizeType scratchSize = (update && !wasRebuilt) ? updateScratchSize : buildScratchSize;

    if (m_scratchBuffer == nullptr)
    {
        m_scratchBuffer = MakeRenderObject<VulkanGpuBuffer>(GpuBufferType::SCRATCH_BUFFER, scratchSize, scratchBufferAlignment);

        HYPERION_PASS_ERRORS(m_scratchBuffer->Create(), result);
    }
    else
    {
        HYPERION_PASS_ERRORS(
            m_scratchBuffer->EnsureCapacity(scratchSize, scratchBufferAlignment),
            result);
    }

    // zero out scratch buffer
    m_scratchBuffer->Memset(m_scratchBuffer->Size(), 0);

    geometryInfo.dstAccelerationStructure = m_accelerationStructure;
    geometryInfo.srcAccelerationStructure = (update && !wasRebuilt) ? m_accelerationStructure : VK_NULL_HANDLE;
    geometryInfo.scratchData = { .deviceAddress = m_scratchBuffer->GetBufferDeviceAddress() };

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> rangeInfos;
    rangeInfos.resize(geometries.size());

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> rangeInfoPtrs;
    rangeInfoPtrs.resize(geometries.size());

    for (SizeType i = 0; i < geometries.size(); i++)
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

    VulkanCommandBufferRef commandBuffer = MakeRenderObject<VulkanCommandBuffer>(CommandBufferType::COMMAND_BUFFER_PRIMARY);
    HYPERION_BUBBLE_ERRORS(commandBuffer->Create(GetRenderBackend()->GetDevice()->GetGraphicsQueue().commandPools[0]));

    HYPERION_BUBBLE_ERRORS(commandBuffer->Begin());

    GetRenderBackend()->GetDevice()->GetFeatures().dynFunctions.vkCmdBuildAccelerationStructuresKHR(
        commandBuffer->GetVulkanHandle(),
        uint32(rangeInfoPtrs.size()),
        &geometryInfo,
        rangeInfoPtrs.data());

    HYPERION_PASS_ERRORS(commandBuffer->End(), result);

    HYPERION_PASS_ERRORS(fence->Create(), result);

    HYPERION_PASS_ERRORS(fence->Reset(), result);

    HYPERION_PASS_ERRORS(commandBuffer->SubmitPrimary(&GetRenderBackend()->GetDevice()->GetGraphicsQueue(), fence, nullptr), result);

    HYPERION_PASS_ERRORS(fence->WaitForGPU(), result);
    HYPERION_PASS_ERRORS(fence->Destroy(), result);

    HYPERION_PASS_ERRORS(commandBuffer->Destroy(), result);

    ClearFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);

    HYPERION_RETURN_OK;
}

RendererResult VulkanAccelerationStructureBase::Destroy()
{
    RendererResult result;

    SafeRelease(std::move(m_geometries));
    SafeRelease(std::move(m_buffer));
    SafeRelease(std::move(m_scratchBuffer));

    if (m_accelerationStructure != VK_NULL_HANDLE)
    {
        GetRenderBackend()->GetDevice()->GetFeatures().dynFunctions.vkDestroyAccelerationStructureKHR(
            GetRenderBackend()->GetDevice()->GetDevice(),
            m_accelerationStructure,
            VK_NULL_HANDLE);

        m_accelerationStructure = VK_NULL_HANDLE;
    }

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

std::vector<VkAccelerationStructureGeometryKHR> VulkanTLAS::GetGeometries() const
{
    return {
        { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {
                .instances = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                    .arrayOfPointers = VK_FALSE,
                    .data = { .deviceAddress = m_instancesBuffer->GetBufferDeviceAddress() } } },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR }
    };
}

std::vector<uint32> VulkanTLAS::GetPrimitiveCounts() const
{
    return { uint32(m_blas.Size()) };
}

RendererResult VulkanTLAS::Create()
{
    HYP_GFX_ASSERT(m_accelerationStructure == VK_NULL_HANDLE);

    if (m_blas.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Top level acceleration structure must have at least one BLAS");
    }

    HYPERION_BUBBLE_ERRORS(CreateOrRebuildInstancesBuffer());

    RTUpdateStateFlags updateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    RendererResult result;

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            GetType(),
            GetGeometries(),
            GetPrimitiveCounts(),
            false,
            updateStateFlags),
        result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    HYP_GFX_ASSERT(updateStateFlags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    HYPERION_PASS_ERRORS(
        CreateMeshDescriptionsBuffer(),
        result);

    updateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

    return result;
}

RendererResult VulkanTLAS::Destroy()
{
    RendererResult result;

    SafeRelease(std::move(m_instancesBuffer));
    SafeRelease(std::move(m_meshDescriptionsBuffer));
    SafeRelease(std::move(m_scratchBuffer));
    SafeRelease(std::move(m_blas));

    HYPERION_PASS_ERRORS(
        VulkanAccelerationStructureBase::Destroy(),
        result);

    return result;
}

void VulkanTLAS::AddBLAS(const BLASRef& blas)
{
    HYP_GFX_ASSERT(blas != nullptr);

    VulkanBLASRef vulkanBlas { blas };

    HYP_GFX_ASSERT(!vulkanBlas->GetGeometries().Empty());

    for (const auto& geometry : vulkanBlas->GetGeometries())
    {
        HYP_GFX_ASSERT(geometry != nullptr);
        HYP_GFX_ASSERT(geometry->GetPackedVerticesBuffer() != nullptr);
        HYP_GFX_ASSERT(geometry->GetPackedIndicesBuffer() != nullptr);
    }

    m_blas.PushBack(std::move(vulkanBlas));

    SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
}

void VulkanTLAS::RemoveBLAS(const BLASRef& blas)
{
    auto it = std::find(m_blas.begin(), m_blas.end(), blas);

    if (it != m_blas.end())
    {
        m_blas.Erase(it);

        SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
    }
}

RendererResult VulkanTLAS::CreateOrRebuildInstancesBuffer()
{
    Threads::AssertOnThread(g_renderThread);

    Array<VkAccelerationStructureInstanceKHR> instances;
    instances.Resize(m_blas.Size());

    for (SizeType i = 0; i < m_blas.Size(); i++)
    {
        const VulkanBLASRef& blas = m_blas[i];
        HYP_GFX_ASSERT(blas != nullptr);

        const uint32 instanceIndex = uint32(i); /* Index of mesh in mesh descriptions buffer. */

        instances[i] = VkAccelerationStructureInstanceKHR {
            .transform = ToVkTransform(blas->GetTransform()),
            .instanceCustomIndex = instanceIndex,
            .mask = 0xff,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR,
            .accelerationStructureReference = blas->GetDeviceAddress()
        };
    }

    constexpr SizeType minInstancesBufferSize = sizeof(VkAccelerationStructureInstanceKHR);

    const SizeType instancesBufferSize = MathUtil::Max(
        minInstancesBufferSize,
        instances.Size() * sizeof(VkAccelerationStructureInstanceKHR));

    if (m_instancesBuffer && m_instancesBuffer->Size() != instancesBufferSize)
    {
        HYP_LOG(RenderingBackend, Info, "Resizing TLAS instances buffer from {} to {}",
            m_instancesBuffer->Size(), instancesBufferSize);

        SafeRelease(std::move(m_instancesBuffer));
    }

    if (!m_instancesBuffer)
    {
        m_instancesBuffer = MakeRenderObject<VulkanGpuBuffer>(GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER, instancesBufferSize);
    }

    if (!m_instancesBuffer->IsCreated())
    {
        HYPERION_BUBBLE_ERRORS(m_instancesBuffer->Create());
    }

    if (instances.Empty())
    {
        // zero out the buffer
        m_instancesBuffer->Memset(instancesBufferSize, 0);
    }
    else
    {
        m_instancesBuffer->Copy(instancesBufferSize, instances.Data());
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanTLAS::UpdateInstancesBuffer(uint32 first, uint32 last)
{
    if (last == first)
    {
        HYPERION_RETURN_OK;
    }

    HYP_GFX_ASSERT(last > first);
    HYP_GFX_ASSERT(first < m_blas.Size());
    HYP_GFX_ASSERT(last <= m_blas.Size());

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

    const SizeType instancesSize = instances.Size() * sizeof(VkAccelerationStructureInstanceKHR);

    HYP_GFX_ASSERT(m_instancesBuffer != nullptr);
    HYP_GFX_ASSERT(m_instancesBuffer->Size() >= instancesSize);

    m_instancesBuffer->Copy(
        first * sizeof(VkAccelerationStructureInstanceKHR),
        instancesSize,
        instances.Data());

    HYPERION_RETURN_OK;
}

RendererResult VulkanTLAS::CreateMeshDescriptionsBuffer()
{
    constexpr SizeType minMeshDescriptionsBufferSize = sizeof(MeshDescription);

    const SizeType meshDescriptionsBufferSize = MathUtil::Max(
        minMeshDescriptionsBufferSize,
        sizeof(MeshDescription) * m_blas.Size());

    m_meshDescriptionsBuffer = MakeRenderObject<VulkanGpuBuffer>(GpuBufferType::SSBO, meshDescriptionsBufferSize);
    HYPERION_BUBBLE_ERRORS(m_meshDescriptionsBuffer->Create());

    // zero out buffer
    m_meshDescriptionsBuffer->Memset(m_meshDescriptionsBuffer->Size(), 0x0);

    if (m_blas.Empty())
    {
        // no need to update the data inside
        HYPERION_RETURN_OK;
    }

    return UpdateMeshDescriptionsBuffer();
}

RendererResult VulkanTLAS::UpdateMeshDescriptionsBuffer()
{
    return UpdateMeshDescriptionsBuffer(0u, uint32(m_blas.Size()));
}

RendererResult VulkanTLAS::UpdateMeshDescriptionsBuffer(uint32 first, uint32 last)
{
    HYP_GFX_ASSERT(m_meshDescriptionsBuffer != nullptr);
    HYP_GFX_ASSERT(m_meshDescriptionsBuffer->Size() >= sizeof(MeshDescription) * m_blas.Size());
    HYP_GFX_ASSERT(first < m_blas.Size());
    HYP_GFX_ASSERT(last <= m_blas.Size());

    if (last <= first)
    {
        // nothing to update
        return RendererResult {};
    }

    Array<MeshDescription> meshDescriptions;
    meshDescriptions.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        const VulkanBLASRef& blas = m_blas[i];

        MeshDescription& meshDescription = meshDescriptions[i - first];
        Memory::MemSet(&meshDescription, 0, sizeof(MeshDescription));

        if (blas->GetGeometries().Empty())
        {
            HYP_GFX_ASSERT(
                false,
                "No geometries added to BLAS node %u!\n",
                i);

            meshDescription = {};
        }
        else
        {
            const Handle<Material>& material = blas->GetGeometries()[0]->GetMaterial();

            if (material.IsValid())
            {
                /// FIXME: This needs to use new resource binding system
                // Must be initialized (AccelerationGeometry calls IncRef() and DecRef())
                HYP_GFX_ASSERT(material->GetRenderResource().GetBufferIndex() != ~0u);
            }

            meshDescription.vertexBufferAddress = blas->GetGeometries()[0]->GetPackedVerticesBuffer()->GetBufferDeviceAddress();
            meshDescription.indexBufferAddress = blas->GetGeometries()[0]->GetPackedIndicesBuffer()->GetBufferDeviceAddress();
            /// FIXME: This needs to use new resource binding system
            meshDescription.materialIndex = material.IsValid() ? material->GetRenderResource().GetBufferIndex() : ~0u;
            meshDescription.numIndices = uint32(blas->GetGeometries()[0]->GetPackedIndicesBuffer()->Size() / sizeof(uint32));
            meshDescription.numVertices = uint32(blas->GetGeometries()[0]->GetPackedVerticesBuffer()->Size() / sizeof(PackedVertex));
        }
    }

    m_meshDescriptionsBuffer->Copy(
        first * sizeof(MeshDescription),
        meshDescriptions.Size() * sizeof(MeshDescription),
        meshDescriptions.Data());

    return RendererResult {};
}

RendererResult VulkanTLAS::RebuildMeshDescriptionsBuffer()
{
    SafeRelease(std::move(m_meshDescriptionsBuffer));

    return CreateMeshDescriptionsBuffer();
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

        if (blas->GetFlags() & ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE)
        {
            dirtyRange |= Range { i, i + 1 };

            blas->ClearFlag(ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
        }

        RTUpdateStateFlags blasUpdateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;
        HYPERION_BUBBLE_ERRORS(blas->UpdateStructure(blasUpdateStateFlags));

        if (blasUpdateStateFlags)
        {
            dirtyRange |= Range { i, i + 1 };
        }
    }

    if (dirtyRange)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS | RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES;

        // update data in instances buffer
        HYPERION_BUBBLE_ERRORS(UpdateInstancesBuffer(
            dirtyRange.GetStart(),
            dirtyRange.GetEnd()));

        // copy mesh descriptions
        HYPERION_BUBBLE_ERRORS(UpdateMeshDescriptionsBuffer(
            dirtyRange.GetStart(),
            dirtyRange.GetEnd()));

        HYPERION_BUBBLE_ERRORS(CreateAccelerationStructure(
            GetType(),
            GetGeometries(),
            GetPrimitiveCounts(),
            true,
            outUpdateStateFlags));
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanTLAS::Rebuild(RTUpdateStateFlags& outUpdateStateFlags)
{
    HYP_GFX_ASSERT(m_accelerationStructure != VK_NULL_HANDLE);

    RendererResult result;

    // check each BLAS, assert that it is valid.
    for (const VulkanBLASRef& blas : m_blas)
    {
        HYP_GFX_ASSERT(blas != nullptr);
        HYP_GFX_ASSERT(!blas->GetGeometries().Empty());

        for (const VulkanAccelerationGeometryRef& geometry : blas->GetGeometries())
        {
            HYP_GFX_ASSERT(geometry != nullptr);
            HYP_GFX_ASSERT(geometry->GetPackedVerticesBuffer() != nullptr);
            HYP_GFX_ASSERT(geometry->GetPackedIndicesBuffer() != nullptr);
        }
    }

    HYPERION_BUBBLE_ERRORS(CreateOrRebuildInstancesBuffer());

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            GetType(),
            GetGeometries(),
            GetPrimitiveCounts(),
            true,
            outUpdateStateFlags),
        result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    HYPERION_PASS_ERRORS(
        RebuildMeshDescriptionsBuffer(),
        result);

    outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

    return result;
}

#pragma endregion TLAS

#pragma region BLAS

VulkanBLAS::VulkanBLAS(
    const VulkanGpuBufferRef& packedVerticesBuffer,
    const VulkanGpuBufferRef& packedIndicesBuffer,
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

RendererResult VulkanBLAS::Create()
{
    RendererResult result;

    std::vector<VkAccelerationStructureGeometryKHR> geometries(m_geometries.Size());
    std::vector<uint32> primitiveCounts(m_geometries.Size());

    if (m_geometries.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create BLAS with zero geometries");
    }

    for (SizeType i = 0; i < m_geometries.Size(); i++)
    {
        const VulkanAccelerationGeometryRef& geometry = m_geometries[i];

        if (!geometry->IsCreated())
        {
            HYPERION_PASS_ERRORS(geometry->Create(), result);
        }

        if (result)
        {
            geometries[i] = geometry->m_geometry;
            primitiveCounts[i] = uint32(geometry->GetPackedIndicesBuffer()->Size() / sizeof(uint32) / 3);

            if (primitiveCounts[i] == 0)
            {
                HYPERION_IGNORE_ERRORS(Destroy());

                return HYP_MAKE_ERROR(RendererError, "Cannot create BLAS -- geometry has zero indices");
            }
        }
        else
        {
            HYPERION_IGNORE_ERRORS(Destroy());

            return result;
        }
    }

    RTUpdateStateFlags updateStateFlags = RT_UPDATE_STATE_FLAGS_NONE;

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            GetType(),
            geometries,
            primitiveCounts,
            false,
            updateStateFlags),
        result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    HYP_GFX_ASSERT(updateStateFlags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    return result;
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

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE)
    {
        outUpdateStateFlags |= RT_UPDATE_STATE_FLAGS_UPDATE_TRANSFORM;

        ClearFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);
    }

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING)
    {
        return Rebuild(outUpdateStateFlags);
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanBLAS::Rebuild(RTUpdateStateFlags& outUpdateStateFlags)
{
    RendererResult result;

    std::vector<VkAccelerationStructureGeometryKHR> geometries(m_geometries.Size());
    std::vector<uint32> primitiveCounts(m_geometries.Size());

    for (SizeType i = 0; i < m_geometries.Size(); i++)
    {
        const VulkanAccelerationGeometryRef& geometry = m_geometries[i];
        HYP_GFX_ASSERT(geometry != nullptr);

        geometries[i] = geometry->m_geometry;
        primitiveCounts[i] = uint32(geometry->GetPackedIndicesBuffer()->Size() / sizeof(uint32) / 3);
    }

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            GetType(),
            geometries,
            primitiveCounts,
            true,
            outUpdateStateFlags),
        result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    m_flags &= ~(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING | ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);

    return result;
}

#pragma endregion BLAS

} // namespace hyperion
