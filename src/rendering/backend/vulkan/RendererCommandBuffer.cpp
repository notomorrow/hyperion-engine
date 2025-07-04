/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererRenderPass.hpp>
#include <rendering/backend/vulkan/RendererSemaphore.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <Types.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanCommandBuffer::VulkanCommandBuffer(CommandBufferType type)
    : m_type(type),
      m_handle(VK_NULL_HANDLE),
      m_commandPool(VK_NULL_HANDLE)
{
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "command buffer should have been destroyed");
}

bool VulkanCommandBuffer::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanCommandBuffer::Create(VkCommandPool commandPool)
{
    if (IsCreated())
    {
        AssertThrowMsg(m_commandPool == commandPool, "Command buffer already created with a different command pool");

        HYPERION_RETURN_OK;
    }

    m_commandPool = commandPool;

    return Create();
}

RendererResult VulkanCommandBuffer::Create()
{
    AssertThrow(m_commandPool != VK_NULL_HANDLE);

    VkCommandBufferLevel level;

    switch (m_type)
    {
    case COMMAND_BUFFER_PRIMARY:
        level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        break;
    case COMMAND_BUFFER_SECONDARY:
        level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        break;
    default:
        HYP_UNREACHABLE();
    }

    VkCommandBufferAllocateInfo allocInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.level = level;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    HYPERION_VK_CHECK_MSG(
        vkAllocateCommandBuffers(GetRenderBackend()->GetDevice()->GetDevice(), &allocInfo, &m_handle),
        "Failed to allocate command buffer");

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::Destroy()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        AssertThrow(m_commandPool != VK_NULL_HANDLE);

        vkFreeCommandBuffers(GetRenderBackend()->GetDevice()->GetDevice(), m_commandPool, 1, &m_handle);

        m_handle = VK_NULL_HANDLE;
        m_commandPool = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::Begin(const VulkanRenderPass* renderPass)
{
    VkCommandBufferInheritanceInfo inheritanceInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
    inheritanceInfo.subpass = 0;
    inheritanceInfo.framebuffer = VK_NULL_HANDLE;

    VkCommandBufferBeginInfo beginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    if (m_type == COMMAND_BUFFER_SECONDARY)
    {
        if (renderPass == nullptr)
        {
            return HYP_MAKE_ERROR(RendererError, "Render pass not provided for secondary command buffer!");
        }

        inheritanceInfo.renderPass = renderPass->GetVulkanHandle();

        beginInfo.pInheritanceInfo = &inheritanceInfo;
        beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }

    if (!m_handle)
    {
        return HYP_MAKE_ERROR(RendererError, "Command buffer not created!");
    }

    HYPERION_VK_CHECK_MSG(
        vkBeginCommandBuffer(m_handle, &beginInfo),
        "Failed to begin command buffer");

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::End()
{
    HYPERION_VK_CHECK_MSG(
        vkEndCommandBuffer(m_handle),
        "Failed to end command buffer");

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::Reset()
{
    HYPERION_VK_CHECK_MSG(
        vkResetCommandBuffer(m_handle, 0),
        "Failed to reset command buffer");

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::SubmitPrimary(
    VulkanDeviceQueue* queue,
    VulkanFence* fence,
    VulkanSemaphoreChain* semaphoreChain)
{
    VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };

    if (semaphoreChain != nullptr)
    {
        submitInfo.waitSemaphoreCount = uint32(semaphoreChain->GetWaitSemaphoresView().size());
        submitInfo.pWaitSemaphores = semaphoreChain->GetWaitSemaphoresView().data();
        submitInfo.signalSemaphoreCount = uint32(semaphoreChain->GetSignalSemaphoresView().size());
        submitInfo.pSignalSemaphores = semaphoreChain->GetSignalSemaphoresView().data();
        submitInfo.pWaitDstStageMask = semaphoreChain->GetWaitSemaphoreStagesView().data();
    }
    else
    {
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = nullptr;
    }

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_handle;

    AssertThrow(fence != nullptr);
    AssertThrow(fence->GetVulkanHandle() != VK_NULL_HANDLE);

    HYPERION_VK_CHECK_MSG(
        vkQueueSubmit(queue->queue, 1, &submitInfo, fence->GetVulkanHandle()),
        "Failed to submit command");

    HYPERION_RETURN_OK;
}

RendererResult VulkanCommandBuffer::SubmitSecondary(VulkanCommandBuffer* primary)
{
    vkCmdExecuteCommands(
        primary->GetVulkanHandle(),
        1,
        &m_handle);

    HYPERION_RETURN_OK;
}

void VulkanCommandBuffer::BindVertexBuffer(const GpuBufferBase* buffer)
{
    AssertThrow(buffer != nullptr);
    AssertThrowMsg(buffer->GetBufferType() == GpuBufferType::MESH_VERTEX_BUFFER, "Not a vertex buffer! Got buffer type: %u", uint32(buffer->GetBufferType()));

    const VkBuffer vertexBuffers[] = { static_cast<const VulkanGpuBuffer*>(buffer)->GetVulkanHandle() };
    static const VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(m_handle, 0, 1, vertexBuffers, offsets);
}

void VulkanCommandBuffer::BindIndexBuffer(const GpuBufferBase* buffer, GpuElemType elemType)
{
    AssertThrow(buffer != nullptr);
    AssertThrowMsg(buffer->GetBufferType() == GpuBufferType::MESH_INDEX_BUFFER, "Not an index buffer! Got buffer type: %u", uint32(buffer->GetBufferType()));

    vkCmdBindIndexBuffer(
        m_handle,
        static_cast<const VulkanGpuBuffer*>(buffer)->GetVulkanHandle(),
        0,
        helpers::ToVkIndexType(elemType));
}

void VulkanCommandBuffer::DrawIndexed(
    uint32 numIndices,
    uint32 numInstances,
    uint32 instanceIndex) const
{
    vkCmdDrawIndexed(
        m_handle,
        numIndices,
        numInstances,
        0,
        0,
        instanceIndex);
}

void VulkanCommandBuffer::DrawIndexedIndirect(
    const GpuBufferBase* buffer,
    uint32 bufferOffset) const
{
    vkCmdDrawIndexedIndirect(
        m_handle,
        static_cast<const VulkanGpuBuffer*>(buffer)->GetVulkanHandle(),
        bufferOffset,
        1,
        uint32(sizeof(IndirectDrawCommand)));
}

void VulkanCommandBuffer::DebugMarkerBegin(const char* markerName) const
{
    if (Features::dynFunctions.vkCmdDebugMarkerBeginEXT)
    {
        const VkDebugMarkerMarkerInfoEXT marker {
            .sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
            .pNext = nullptr,
            .pMarkerName = markerName
        };

        Features::dynFunctions.vkCmdDebugMarkerBeginEXT(m_handle, &marker);
    }
}

void VulkanCommandBuffer::DebugMarkerEnd() const
{
    if (Features::dynFunctions.vkCmdDebugMarkerEndEXT)
    {
        Features::dynFunctions.vkCmdDebugMarkerEndEXT(m_handle);
    }
}

} // namespace hyperion