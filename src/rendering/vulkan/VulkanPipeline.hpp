/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/RenderObject.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
extern HYP_API VkDescriptorSetLayout GetVkDescriptorSetLayout(const VulkanDescriptorSetLayoutWrapper& layout);

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
    HYP_API VulkanPipelineBase();

    HYP_FORCE_INLINE VkPipeline GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkPipelineLayout GetVulkanPipelineLayout() const
    {
        return m_layout;
    }

    HYP_API Array<VkDescriptorSetLayout> GetDescriptorSetLayouts() const;

    HYP_API RendererResult Destroy();

    HYP_API bool IsCreated() const;

    HYP_API void SetPushConstants(const void* data, SizeType size);

#ifdef HYP_DEBUG_MODE
    HYP_API void SetDebugName(Name name);
#endif

protected:
    VkPipeline m_handle;
    VkPipelineLayout m_layout;

    PushConstantData m_pushConstants;
};

} // namespace hyperion
