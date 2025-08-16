/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

VkDescriptorSetLayout GetVkDescriptorSetLayout(const VulkanDescriptorSetLayoutWrapper& layout);

template <class PipelineType>
static inline Array<VkDescriptorSetLayout> GetPipelineVulkanDescriptorSetLayouts(const PipelineType& pipeline)
{
    Assert(pipeline.GetDescriptorTable().IsValid(), "Invalid DescriptorTable provided to Pipeline");

    Array<VkDescriptorSetLayout> usedLayouts;

    for (const DescriptorSetRef& descriptorSet : pipeline.GetDescriptorTable()->GetSets()[0])
    {
        VulkanDescriptorSetRef vulkanDescriptorSet = VulkanDescriptorSetRef(descriptorSet);

        Assert(vulkanDescriptorSet != nullptr);
        Assert(vulkanDescriptorSet->GetVulkanLayoutWrapper() != nullptr);

        usedLayouts.PushBack(GetVkDescriptorSetLayout(*vulkanDescriptorSet->GetVulkanLayoutWrapper()));
    }

    return usedLayouts;
}

class VulkanPipelineBase
{
public:
    VulkanPipelineBase();
    ~VulkanPipelineBase();

    HYP_FORCE_INLINE VkPipeline GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkPipelineLayout GetVulkanPipelineLayout() const
    {
        return m_layout;
    }

    Array<VkDescriptorSetLayout> GetDescriptorSetLayouts() const;

    bool IsCreated() const;

    void SetPushConstants(const void* data, SizeType size);

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name);
#endif

protected:
    VkPipeline m_handle;
    VkPipelineLayout m_layout;

    PushConstantData m_pushConstants;
};

} // namespace hyperion
