/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderQueue.hpp>
#include <rendering/RenderFrame.hpp>

#if defined(HYP_DEBUG_MODE) && defined(HYP_VULKAN)
// for debugging
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanGraphicsPipeline.hpp>
#endif

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

#pragma region RenderQueue

RenderQueue::RenderQueue()
{
}

RenderQueue::~RenderQueue()
{
    for (CmdBase* command : m_commands)
    {
        FreeCommand(command);
    }
}

void RenderQueue::Prepare(FrameBase* frame)
{
    Assert(frame != nullptr);

    for (CmdBase* command : m_commands)
    {
        command->Prepare(frame);
    }
}

void RenderQueue::Execute(const CommandBufferRef& cmd)
{
    Assert(cmd != nullptr);

    for (CmdBase* command : m_commands)
    {
        command->Execute(cmd);

        FreeCommand(command);
    }

    m_commands.Clear();
}

void RenderQueue::FreeCommand(CmdBase* command)
{
    Assert(command != nullptr);

    CmdMemoryPoolBase* pool = command->m_poolHandle.pool;
    Assert(pool != nullptr);

    pool->FreeCommand(command);
}

#pragma endregion RenderQueue

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

BeginFramebuffer::BeginFramebuffer(const FramebufferRef& framebuffer)
    : m_framebuffer(framebuffer)
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

EndFramebuffer::EndFramebuffer(const FramebufferRef& framebuffer)
    : m_framebuffer(framebuffer)
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
}

BindGraphicsPipeline::BindGraphicsPipeline(const GraphicsPipelineRef& pipeline)
    : m_pipeline(pipeline)
{
    Assert(g_activeFramebuffer != nullptr, "Cannot bind graphics pipeline: not in a framebuffer");
}

#endif

#pragma endregion BindGraphicsPipeline

} // namespace hyperion
