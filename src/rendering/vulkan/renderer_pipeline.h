//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_PIPELINE_H
#define HYPERION_RENDERER_PIPELINE_H

#include <vulkan/vulkan.h>

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_buffer.h"
#include "renderer_shader.h"

namespace hyperion {

class RendererPipeline {
    std::vector<VkVertexInputAttributeDescription> SetVertexAttribs();

public:
    RendererPipeline(RendererDevice *_device, RendererSwapchain *_swapchain);
    void Destroy();

    void SetPrimitive(VkPrimitiveTopology _primitive);
    void SetDynamicStates(const std::vector<VkDynamicState> &_states);

    void CreateCommandPool();
    void CreateCommandBuffers();

    void UpdateDynamicStates(VkCommandBuffer *cmd);
    void SetViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f);
    void SetScissor(int x, int y, uint32_t width, uint32_t height);
    void SetVertexInputMode(std::vector<VkVertexInputBindingDescription> &binding_descs, std::vector<VkVertexInputAttributeDescription> &vertex_attribs);
    void Rebuild(RendererShader *shader);

    void CreateRenderPass(VkSampleCountFlagBits sample_count=VK_SAMPLE_COUNT_1_BIT);
    // void DoRenderPass(void (*render_callback)(RendererPipeline *pl, VkCommandBuffer *cmd));
    void StartRenderPass(VkCommandBuffer *cmd=nullptr);
    void EndRenderPass(VkCommandBuffer *cmd=nullptr);

    VkPrimitiveTopology GetPrimitive();
    std::vector<VkDynamicState> GetDynamicStates();
    VkRenderPass *GetRenderPass();

    /* Per frame data */
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;

    VkPipeline pipeline;
private:
    VkBuffer *intern_vertex_buffers = nullptr;
    uint64_t  intern_vertex_buffers_size = 0;

    std::vector<VkDynamicState> dynamic_states;

    VkViewport viewport;
    VkRect2D scissor;
    VkPrimitiveTopology primitive;

    VkPipelineLayout layout;
    VkRenderPass render_pass;

    std::vector<VkVertexInputBindingDescription>   vertex_binding_descriptions = { };
    std::vector<VkVertexInputAttributeDescription> vertex_attributes = { };

    RendererSwapchain *swapchain;
    RendererDevice *device;

    void SetVertexBuffers(std::vector<RendererVertexBuffer> &vertex_buffers);
};

}; /* namespace hyperion */

#endif //HYPERION_RENDERER_PIPELINE_H
