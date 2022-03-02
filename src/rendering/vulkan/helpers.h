#ifndef HYPERION_RENDERER_HELPERS_H
#define HYPERION_RENDERER_HELPERS_H

#include <vulkan/vulkan.h>

namespace hyperion {
namespace helpers {
// https://vulkan-tutorial.com/Texture_mapping/Images
class SingleTimeCommands {
public:
    template <class LambdaFunction>
    inline RendererResult Execute(RendererDevice *device, LambdaFunction execute_commands)
    {
        Begin(device);

        auto result = execute_commands(cmd);

        End(device);

        return result;
    }

    inline
    RendererResult TransitionImageLayout(RendererDevice *device, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
        return Execute(device, [this, image, format, old_layout, new_layout](VkCommandBuffer cmd) -> RendererResult {
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

            vkCmdPipelineBarrier(cmd, src, dst, {}, 0, nullptr, 0, nullptr, 1, &barrier);
            
            return RendererResult(RendererResult::RENDERER_OK);
        });
    }

    VkCommandBuffer cmd;
    VkCommandPool pool;
    QueueFamilyIndices family_indices;

private:
    inline VkCommandBuffer Begin(RendererDevice *device)
    {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = pool;
        alloc_info.commandBufferCount = 1;

        vkAllocateCommandBuffers(device->GetDevice(), &alloc_info, &cmd);

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &begin_info);

        return cmd;
    }

    inline void End(RendererDevice *device)
    {
        auto queue_graphics = device->GetQueue(family_indices.graphics_family.value(), 0);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        vkQueueSubmit(queue_graphics, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue_graphics);

        vkFreeCommandBuffers(device->GetDevice(), pool, 1, &cmd);
    }
};


} // namespace helpers
} // namespace hyperion

#endif