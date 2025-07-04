/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/rhi/CmdList.hpp>
#include <rendering/backend/RendererFrame.hpp>

#if defined(HYP_DEBUG_MODE) && defined(HYP_VULKAN)
// for debugging
#include <rendering/backend/vulkan/RendererFramebuffer.hpp>
#include <rendering/backend/vulkan/RendererGraphicsPipeline.hpp>
#endif

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

#pragma region CmdList

CmdList::CmdList()
{
}

CmdList::~CmdList()
{
    for (CmdBase* command : m_commands)
    {
        FreeCommand(command);
    }
}

void CmdList::Prepare(FrameBase* frame)
{
    Assert(frame != nullptr);

    for (CmdBase* command : m_commands)
    {
        command->Prepare(frame);
    }
}

void CmdList::Execute(const CommandBufferRef& cmd)
{
    Assert(cmd != nullptr);

    for (CmdBase* command : m_commands)
    {
        command->Execute(cmd);

        FreeCommand(command);
    }

    m_commands.Clear();
}

void CmdList::FreeCommand(CmdBase* command)
{
    Assert(command != nullptr);

    CmdMemoryPoolBase* pool = command->m_poolHandle.pool;
    Assert(pool != nullptr);

    pool->FreeCommand(command);
}

#pragma endregion CmdList

#pragma region BindDescriptorSet

void BindDescriptorSet::Prepare(FrameBase* frame)
{
    Assert(m_descriptorSet->IsCreated());

    frame->MarkDescriptorSetUsed(m_descriptorSet);
}

#pragma endregion BindDescriptorSet

#pragma region BindDescriptorTable

void BindDescriptorTable::Prepare(FrameBase* frame)
{
    for (const DescriptorSetRef& descriptorSet : m_descriptorTable->GetSets()[frame->GetFrameIndex()])
    {
        if (descriptorSet->GetLayout().IsTemplate())
        {
            continue;
        }

        Assert(descriptorSet->IsCreated());

        frame->MarkDescriptorSetUsed(descriptorSet);
    }
}

#pragma endregion BindDescriptorTable

#pragma region BeginFramebuffer

#ifdef HYP_DEBUG_MODE
static FramebufferBase* g_activeFramebuffer = nullptr;

BeginFramebuffer::BeginFramebuffer(const FramebufferRef& framebuffer, uint32 frameIndex)
    : m_framebuffer(framebuffer),
      m_frameIndex(frameIndex)
{
    Assert(!g_activeFramebuffer, "Cannot begin framebuffer: already in a framebuffer");
    g_activeFramebuffer = framebuffer.Get();
}

void BeginFramebuffer::Prepare(FrameBase* frame)
{
}
#endif

#pragma endregion BeginFramebuffer

#pragma region EndFramebuffer

#ifdef HYP_DEBUG_MODE

EndFramebuffer::EndFramebuffer(const FramebufferRef& framebuffer, uint32 frameIndex)
    : m_framebuffer(framebuffer),
      m_frameIndex(frameIndex)
{
    Assert(g_activeFramebuffer, "Cannot end framebuffer: not in a framebuffer");
    Assert(g_activeFramebuffer == framebuffer.Get(), "Cannot end framebuffer: mismatched framebuffer");
    g_activeFramebuffer = nullptr;
}

void EndFramebuffer::Prepare(FrameBase* frame)
{
}
#endif

#pragma endregion EndFramebuffer

#pragma region BindGraphicsPipeline

#ifdef HYP_DEBUG_MODE

BindGraphicsPipeline::BindGraphicsPipeline(const GraphicsPipelineRef& pipeline, Vec2i viewportOffset, Vec2u viewportExtent)
    : m_pipeline(pipeline),
      m_viewportOffset(viewportOffset),
      m_viewportExtent(viewportExtent)
{
    Assert(g_activeFramebuffer != nullptr, "Cannot bind graphics pipeline: not in a framebuffer");
    //     Assert(pipeline->GetFramebuffers().FindAs(g_activeFramebuffer) != pipeline->GetFramebuffers().End(), "Cannot bind graphics pipeline: framebuffer does not match pipeline");
    // #ifdef HYP_VULKAN
    //     VulkanFramebuffer* vulkanFramebuffer = static_cast<VulkanFramebuffer*>(g_activeFramebuffer);
    //     VulkanGraphicsPipeline* vulkanPipeline = static_cast<VulkanGraphicsPipeline*>(pipeline.Get());

    //     Assert(vulkanFramebuffer->GetRenderPass()->GetAttachments()[0]->GetFormat() == vulkanPipeline->GetRenderPass()->GetAttachments()[0]->GetFormat(),
    //         "Cannot bind graphics pipeline: render pass does not match framebuffer render pass");
    // #endif
}

BindGraphicsPipeline::BindGraphicsPipeline(const GraphicsPipelineRef& pipeline)
    : m_pipeline(pipeline)
{
    Assert(g_activeFramebuffer != nullptr, "Cannot bind graphics pipeline: not in a framebuffer");
    //     Assert(pipeline->GetFramebuffers().FindAs(g_activeFramebuffer) != pipeline->GetFramebuffers().End(), "Cannot bind graphics pipeline: framebuffer does not match pipeline");
    // #ifdef HYP_VULKAN
    //     VulkanFramebuffer* vulkanFramebuffer = static_cast<VulkanFramebuffer*>(g_activeFramebuffer);
    //     VulkanGraphicsPipeline* vulkanPipeline = static_cast<VulkanGraphicsPipeline*>(pipeline.Get());

    //     Assert(vulkanFramebuffer->GetRenderPass()->GetAttachments()[0]->GetFormat() == vulkanPipeline->GetRenderPass()->GetAttachments()[0]->GetFormat(),
    //         "Cannot bind graphics pipeline: render pass does not match framebuffer render pass");
    // #endif
}

#endif

#pragma endregion BindGraphicsPipeline

} // namespace hyperion
