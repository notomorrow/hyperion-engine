#ifndef HYPERION_RENDERER_HELPERS_H
#define HYPERION_RENDERER_HELPERS_H

#include "renderer_result.h"
#include "renderer_device.h"
#include "renderer_structs.h"
#include "renderer_command_buffer.h"

#include <vulkan/vulkan.h>

#include <vector>

#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace hyperion {
namespace renderer {
namespace helpers {

uint32_t MipmapSize(uint32_t src_size, int lod);

VkIndexType ToVkIndexType(DatumType);

class SingleTimeCommands {
public:
    SingleTimeCommands() : command_buffer{}, pool{}, family_indices{} {}

    inline void Push(const std::function<Result(CommandBuffer *)> &fn)
    {
        m_functions.push_back(fn);
    }

    inline Result Execute(Device *device)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device));

        auto result = Result::OK;

        for (auto &fn : m_functions) {
            HYPERION_PASS_ERRORS(fn(command_buffer.get()), result);

            if (!result) {
                break;
            }
        }

        m_functions.clear();

        HYPERION_BUBBLE_ERRORS(End(device), result);

        return result;
    }

    std::unique_ptr<CommandBuffer> command_buffer;
    VkCommandPool pool;
    QueueFamilyIndices family_indices;

private:
    std::vector<std::function<Result(CommandBuffer *)>> m_functions;

    inline Result Begin(Device *device)
    {
        command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_PRIMARY);

        HYPERION_BUBBLE_ERRORS(command_buffer->Create(device, pool));

        return command_buffer->Begin(device);
    }

    inline Result End(Device *device)
    {
        auto result = Result::OK;

        HYPERION_PASS_ERRORS(command_buffer->End(device), result);

        // Create fence to ensure that the command buffer has finished executing
        VkFenceCreateInfo fence_create_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};

        VkFence fence;

        HYPERION_VK_CHECK(vkCreateFence(device->GetDevice(), &fence_create_info, nullptr, &fence));

        // Submit to the queue
        auto queue_graphics = device->GetQueue(family_indices.graphics_family.value(), 0);

        HYPERION_PASS_ERRORS(command_buffer->SubmitPrimary(queue_graphics, fence, nullptr), result);
        
        // Wait for the fence to signal that command buffer has finished executing
        HYPERION_VK_PASS_ERRORS(vkWaitForFences(device->GetDevice(), 1, &fence, true, DEFAULT_FENCE_TIMEOUT), result);

        vkDestroyFence(device->GetDevice(), fence, nullptr);

        HYPERION_PASS_ERRORS(command_buffer->Destroy(device, pool), result);

        return result;
    }
};

} // namespace helpers
} // namespace renderer
} // namespace hyperion

#endif