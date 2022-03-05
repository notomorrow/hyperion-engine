#ifndef HYPERION_RENDERER_HELPERS_H
#define HYPERION_RENDERER_HELPERS_H

#include "renderer_result.h"
#include "renderer_device.h"

#include <vulkan/vulkan.h>

#include <vector>

#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace hyperion {
namespace helpers {

class SingleTimeCommands {
public:
    inline void Push(const std::function<RendererResult(VkCommandBuffer)> &fn)
    {
        m_functions.push_back(fn);
    }

    inline RendererResult Execute(RendererDevice *device)
    {
        auto begin_result = Begin(device);
        if (!begin_result) return begin_result;

        RendererResult result(RendererResult::RENDERER_OK);

        for (auto &fn : m_functions) {
            result = fn(cmd);

            if (!result) {
                break;
            }
        }

        m_functions.clear();

        auto end_result = End(device);
        if (!end_result) return end_result;

        return result;
    }

    inline void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
        Push([this, image, format, old_layout, new_layout](VkCommandBuffer cmd) -> RendererResult {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            VkPipelineStageFlags src;
            VkPipelineStageFlags dst;

            if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                DebugLog(LogType::Info, "Transitioning image layout from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL");
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
            } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
                DebugLog(LogType::Info, "Transitioning image layout from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL");
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
            } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                DebugLog(LogType::Info, "Transitioning image layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL");

                if (this->family_indices.transfer_family != this->family_indices.graphics_family) {
                    barrier.srcQueueFamilyIndex = this->family_indices.transfer_family.value();
                    barrier.dstQueueFamilyIndex = this->family_indices.graphics_family.value();
                } else {
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                }
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                src = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            } else {
                return RendererResult(RendererResult::RENDERER_ERR, "Unsupported image layout transition");
            }

            vkCmdPipelineBarrier(cmd, src, dst, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            
            return RendererResult(RendererResult::RENDERER_OK);
        });
    }

    VkCommandBuffer cmd;
    VkCommandPool pool;
    QueueFamilyIndices family_indices;

private:
    std::vector<std::function<RendererResult(VkCommandBuffer)>> m_functions;

    inline RendererResult Begin(RendererDevice *device)
    {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = pool;
        alloc_info.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device->GetDevice(), &alloc_info, &cmd) != VK_SUCCESS) {
            return RendererResult(RendererResult::RENDERER_ERR, "Failed to allocate command buffers");
        }

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS) {
            vkFreeCommandBuffers(device->GetDevice(), pool, 1, &cmd);
            return RendererResult(RendererResult::RENDERER_ERR, "Failed to begin command buffer");
        }

        return RendererResult(RendererResult::RENDERER_OK);
    }

    inline RendererResult End(RendererDevice *device)
    {
        auto queue_graphics = device->GetQueue(family_indices.graphics_family.value(), 0);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            return RendererResult(RendererResult::RENDERER_ERR, "Failed to end command buffer");
        }

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        // Create fence to ensure that the command buffer has finished executing
        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = 0;

        VkFence fence;
        if (vkCreateFence(device->GetDevice(), &fence_create_info, nullptr, &fence) != VK_SUCCESS) {
            return RendererResult(RendererResult::RENDERER_ERR, "Failed to create fence");
        }

        // Submit to the queue
        if (vkQueueSubmit(queue_graphics, 1, &submit_info, fence) != VK_SUCCESS) {
            return RendererResult(RendererResult::RENDERER_ERR, "Failed to submit fence to queue");
        }

        // Wait for the fence to signal that command buffer has finished executing
        if (vkWaitForFences(device->GetDevice(), 1, &fence, true, DEFAULT_FENCE_TIMEOUT) != VK_SUCCESS) {
            return RendererResult(RendererResult::RENDERER_ERR, "Failed to wait for fences");
        }

        vkDestroyFence(device->GetDevice(), fence, nullptr);
        vkFreeCommandBuffers(device->GetDevice(), pool, 1, &cmd);

        return RendererResult(RendererResult::RENDERER_OK);
    }
};


} // namespace helpers
} // namespace hyperion

#endif