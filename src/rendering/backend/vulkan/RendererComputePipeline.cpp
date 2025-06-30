/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererComputePipeline.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererShader.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Transform.hpp>

#include <cstring>

#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanComputePipeline::VulkanComputePipeline()
    : VulkanPipelineBase(),
      ComputePipelineBase()
{
}

VulkanComputePipeline::VulkanComputePipeline(const VulkanShaderRef& shader, const VulkanDescriptorTableRef& descriptorTable)
    : VulkanPipelineBase(),
      ComputePipelineBase(shader, descriptorTable)
{
}

VulkanComputePipeline::~VulkanComputePipeline()
{
}

void VulkanComputePipeline::Bind(CommandBufferBase* commandBuffer)
{
    AssertThrow(m_handle != VK_NULL_HANDLE);

    vkCmdBindPipeline(
        static_cast<VulkanCommandBuffer*>(commandBuffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_handle);

    if (m_pushConstants)
    {
        vkCmdPushConstants(
            static_cast<VulkanCommandBuffer*>(commandBuffer)->GetVulkanHandle(),
            m_layout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            m_pushConstants.Size(),
            m_pushConstants.Data());
    }
}

void VulkanComputePipeline::Dispatch(
    CommandBufferBase* commandBuffer,
    const Vec3u& groupSize) const
{
    vkCmdDispatch(
        static_cast<VulkanCommandBuffer*>(commandBuffer)->GetVulkanHandle(),
        groupSize.x,
        groupSize.y,
        groupSize.z);
}

void VulkanComputePipeline::DispatchIndirect(
    CommandBufferBase* commandBuffer,
    const GpuBufferRef& indirectBuffer,
    SizeType offset) const
{
    vkCmdDispatchIndirect(
        static_cast<VulkanCommandBuffer*>(commandBuffer)->GetVulkanHandle(),
        static_cast<VulkanGpuBuffer*>(indirectBuffer.Get())->GetVulkanHandle(),
        offset);
}

RendererResult VulkanComputePipeline::Create()
{
    /* Push constants */
    const VkPushConstantRange pushConstantRanges[] = {
        { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = uint32(GetRenderBackend()->GetDevice()->GetFeatures().PaddedSize<PushConstantData>()) }
    };

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const Array<VkDescriptorSetLayout> usedLayouts = GetPipelineVulkanDescriptorSetLayouts(*this);
    const uint32 maxSetLayouts = GetRenderBackend()->GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

#if 1
    HYP_LOG(RenderingBackend, Debug, "Using {} descriptor set layouts in pipeline", usedLayouts.Size());

    for (const DescriptorSetRef& descriptorSet : m_descriptorTable->GetSets()[0])
    {
        HYP_LOG(RenderingBackend, Debug, "\tDescriptor set layout: {} ({})",
            descriptorSet->GetLayout().GetName(), descriptorSet->GetLayout().GetDeclaration()->setIndex);

        for (const auto& it : descriptorSet->GetLayout().GetElements())
        {
            HYP_LOG(RenderingBackend, Debug, "\t\tDescriptor: {}  binding: {}",
                it.first, it.second.binding);
        }
    }
#endif

    if (usedLayouts.Size() > maxSetLayouts)
    {
        return HYP_MAKE_ERROR(RendererError, "Device max bound descriptor sets exceeded");
    }

    layoutInfo.setLayoutCount = uint32(usedLayouts.Size());
    layoutInfo.pSetLayouts = usedLayouts.Data();
    layoutInfo.pushConstantRangeCount = uint32(std::size(pushConstantRanges));
    layoutInfo.pPushConstantRanges = pushConstantRanges;

    HYPERION_VK_CHECK_MSG(
        vkCreatePipelineLayout(GetRenderBackend()->GetDevice()->GetDevice(), &layoutInfo, nullptr, &m_layout),
        "Failed to create compute pipeline layout");

    VkComputePipelineCreateInfo pipelineInfo { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

    if (!m_shader)
    {
        return HYP_MAKE_ERROR(RendererError, "Compute shader not provided to pipeline");
    }

    const Array<VkPipelineShaderStageCreateInfo>& stages = static_cast<VulkanShader*>(m_shader.Get())->GetVulkanShaderStages();

    if (stages.Size() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Compute pipelines must have at least one shader stage");
    }

    if (stages.Size() > 1)
    {
        return HYP_MAKE_ERROR(RendererError, "Compute pipelines must have only one shader stage");
    }

    pipelineInfo.stage = stages.Front();
    pipelineInfo.layout = m_layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    HYPERION_VK_CHECK_MSG(
        vkCreateComputePipelines(GetRenderBackend()->GetDevice()->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_handle),
        "Failed to create compute pipeline");

    HYPERION_RETURN_OK;
}

RendererResult VulkanComputePipeline::Destroy()
{
    SafeRelease(std::move(m_shader));
    SafeRelease(std::move(m_descriptorTable));

    return VulkanPipelineBase::Destroy();
}

void VulkanComputePipeline::SetPushConstants(const void* data, SizeType size)
{
    VulkanPipelineBase::SetPushConstants(data, size);
}

} // namespace hyperion
