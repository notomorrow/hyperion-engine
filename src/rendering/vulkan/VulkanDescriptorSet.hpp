/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/utilities/Optional.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/threading/Mutex.hpp>
#include <core/Defines.hpp>

#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderObject.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

class VulkanDescriptorSetLayoutWrapper;
using VulkanDescriptorSetLayoutWrapperRef = RenderObjectHandle_Strong<VulkanDescriptorSetLayoutWrapper>;

struct VulkanDescriptorElementInfo
{
    uint32 binding;
    uint32 index;
    VkDescriptorType descriptorType;

    union
    {
        VkDescriptorBufferInfo bufferInfo;
        VkDescriptorImageInfo imageInfo;
        VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureInfo;
    };
};

class VulkanDescriptorSet final : public DescriptorSetBase
{
    using ElementCache = HashMap<Name, Array<VulkanDescriptorElementInfo>, HashTable_DynamicNodeAllocator<KeyValuePair<Name, Array<VulkanDescriptorElementInfo>>>>;

public:
    HYP_API VulkanDescriptorSet(const DescriptorSetLayout& layout);
    HYP_API ~VulkanDescriptorSet();

    HYP_FORCE_INLINE VkDescriptorSet GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VulkanDescriptorSetLayoutWrapper* GetVulkanLayoutWrapper() const
    {
        return m_vkLayoutWrapper.Get();
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual void UpdateDirtyState(bool* outIsDirty = nullptr) override;
    HYP_API virtual void Update(bool force = false) override;

    HYP_API virtual DescriptorSetRef Clone() const override;

protected:
    virtual void Bind(const CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, uint32 bindIndex) const override;
    virtual void Bind(const CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex) const override;
    virtual void Bind(const CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, uint32 bindIndex) const override;
    virtual void Bind(const CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex) const override;
    virtual void Bind(const CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, uint32 bindIndex) const override;
    virtual void Bind(const CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bindIndex) const override;

    VkDescriptorSet m_handle;
    ElementCache m_cachedElements;
    VulkanDescriptorSetLayoutWrapperRef m_vkLayoutWrapper;
    Array<VulkanDescriptorElementInfo> m_vkDescriptorElementInfos;
};

class VulkanDescriptorTable : public DescriptorTableBase
{
public:
    HYP_API VulkanDescriptorTable(const DescriptorTableDeclaration* decl);
    HYP_API virtual ~VulkanDescriptorTable() override = default;
};

} // namespace hyperion
