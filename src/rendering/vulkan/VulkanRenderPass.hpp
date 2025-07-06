/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_RENDER_PASS_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_RENDER_PASS_HPP

#include <rendering/backend/vulkan/RendererImage.hpp>
#include <rendering/backend/vulkan/RendererImageView.hpp>
#include <rendering/backend/vulkan/RendererSampler.hpp>
#include <rendering/backend/vulkan/RendererAttachment.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/Vector4.hpp>
#include <core/containers/Array.hpp>

#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
enum RenderPassMode
{
    RENDER_PASS_INLINE = 0,
    RENDER_PASS_SECONDARY_COMMAND_BUFFER = 1
};

class VulkanRenderPass final : public RenderObject<VulkanRenderPass>
{
public:
    VulkanRenderPass(RenderPassStage stage, RenderPassMode mode);
    VulkanRenderPass(RenderPassStage stage, RenderPassMode mode, uint32 numMultiviewLayers);
    virtual ~VulkanRenderPass() override;

    HYP_FORCE_INLINE VkRenderPass GetVulkanHandle() const
    {
        return m_handle;
    }

    RenderPassStage GetStage() const
    {
        return m_stage;
    }

    bool IsMultiview() const
    {
        return m_numMultiviewLayers > 1;
    }

    uint32 NumMultiviewLayers() const
    {
        return m_numMultiviewLayers;
    }

    void AddAttachment(VulkanAttachmentRef attachment);
    bool RemoveAttachment(const VulkanAttachment* attachment);

    const Array<VulkanAttachmentRef>& GetAttachments() const
    {
        return m_renderPassAttachments;
    }

    RendererResult Create();
    RendererResult Destroy();

    void Begin(VulkanCommandBuffer* cmd, VulkanFramebuffer* framebuffer, uint32 frameIndex);
    void End(VulkanCommandBuffer* cmd);

private:
    void CreateDependencies();

    void AddDependency(const VkSubpassDependency& dependency)
    {
        m_dependencies.PushBack(dependency);
    }

    RenderPassStage m_stage;
    RenderPassMode m_mode;
    uint32 m_numMultiviewLayers;

    Array<VulkanAttachmentRef> m_renderPassAttachments;

    Array<VkSubpassDependency> m_dependencies;
    Array<VkClearValue> m_vkClearValues;

    VkRenderPass m_handle;
};

using VulkanRenderPassRef = RenderObjectHandle_Strong<VulkanRenderPass>;
using VulkanRenderPassWeakRef = RenderObjectHandle_Weak<VulkanRenderPass>;

} // namespace hyperion

#endif