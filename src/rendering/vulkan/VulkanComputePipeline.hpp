/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_COMPUTE_PIPELINE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_COMPUTE_PIPELINE_HPP

#include <rendering/backend/RendererComputePipeline.hpp>

#include <rendering/backend/vulkan/RendererPipeline.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanComputePipeline final : public ComputePipelineBase, public VulkanPipelineBase
{
public:
    VulkanComputePipeline();
    VulkanComputePipeline(const VulkanShaderRef& shader, const VulkanDescriptorTableRef& descriptorTable);
    virtual ~VulkanComputePipeline() override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_API virtual void Bind(CommandBufferBase* commandBuffer) override;

    HYP_API virtual void Dispatch(CommandBufferBase* commandBuffer, const Vec3u& groupSize) const override;
    HYP_API virtual void DispatchIndirect(
        CommandBufferBase* commandBuffer,
        const GpuBufferRef& indirectBuffer,
        SizeType offset = 0) const override;

    HYP_API virtual void SetPushConstants(const void* data, SizeType size) override;
};

} // namespace hyperion

#endif // HYPERION_RENDERER_BACKEND_VULKAN_COMPUTE_PIPELINE_HPP
