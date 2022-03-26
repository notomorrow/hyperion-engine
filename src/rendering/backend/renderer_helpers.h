#ifndef HYPERION_RENDERER_HELPERS_H
#define HYPERION_RENDERER_HELPERS_H

#include "renderer_result.h"
#include "renderer_device.h"
#include "renderer_structs.h"

#include <vulkan/vulkan.h>

#include <vector>

#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace hyperion {
namespace renderer {
namespace helpers {

size_t MipmapSize(size_t src_size, int lod);

VkIndexType ToVkIndexType(DatumType);

class SingleTimeCommands {
public:
    SingleTimeCommands() : cmd{}, pool{}, family_indices{} {}

    inline void Push(const std::function<Result(VkCommandBuffer)> &fn)
    {
        m_functions.push_back(fn);
    }

    inline Result Execute(Device *device)
    {
        auto begin_result = Begin(device);
        if (!begin_result) return begin_result;

        Result result(Result::RENDERER_OK);

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

    VkCommandBuffer cmd;
    VkCommandPool pool;
    QueueFamilyIndices family_indices;

private:
    std::vector<std::function<Result(VkCommandBuffer)>> m_functions;

    inline Result Begin(Device *device)
    {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = pool;
        alloc_info.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device->GetDevice(), &alloc_info, &cmd) != VK_SUCCESS) {
            return Result(Result::RENDERER_ERR, "Failed to allocate command buffers");
        }

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS) {
            vkFreeCommandBuffers(device->GetDevice(), pool, 1, &cmd);
            return Result(Result::RENDERER_ERR, "Failed to begin command buffer");
        }

        HYPERION_RETURN_OK;
    }

    inline Result End(Device *device)
    {
        auto queue_graphics = device->GetQueue(family_indices.graphics_family.value(), 0);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            return Result(Result::RENDERER_ERR, "Failed to end command buffer");
        }

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        // Create fence to ensure that the command buffer has finished executing
        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = 0;

        auto result = Result::OK;

        VkFence fence;

        HYPERION_VK_CHECK(vkCreateFence(device->GetDevice(), &fence_create_info, nullptr, &fence));

        // Submit to the queue
        HYPERION_VK_PASS_ERRORS(vkQueueSubmit(queue_graphics, 1, &submit_info, fence), result);

        // Wait for the fence to signal that command buffer has finished executing
        HYPERION_VK_PASS_ERRORS(vkWaitForFences(device->GetDevice(), 1, &fence, true, DEFAULT_FENCE_TIMEOUT), result);

        vkDestroyFence(device->GetDevice(), fence, nullptr);
        vkFreeCommandBuffers(device->GetDevice(), pool, 1, &cmd);

        return result;
    }
};

} // namespace helpers
} // namespace renderer
} // namespace hyperion

#endif