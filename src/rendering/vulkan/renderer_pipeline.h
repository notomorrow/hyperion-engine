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
#include "renderer_render_pass.h"
#include "renderer_descriptor_pool.h"
#include "renderer_descriptor_set.h"
#include "renderer_descriptor.h"
#include "renderer_helpers.h"

#include "../../hash_code.h"

namespace hyperion {
class RendererFramebufferObject;
class RendererPipeline {
public:
    struct ConstructionInfo {
        RendererMeshInputAttributeSet vertex_attributes;
        RendererShader *shader;
        std::unique_ptr<RendererRenderPass> render_pass;
        std::vector<std::unique_ptr<RendererFramebufferObject>> fbos;

        enum class CullMode : int {
            NONE,
            BACK,
            FRONT
        } cull_mode;

        bool depth_test,
             depth_write;

        inline HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add((shader != nullptr) ? shader->GetHashCode().Value() : 0);
            hc.Add(intptr_t(render_pass.get())); // TODO
            for (auto &fbo : fbos) {
                hc.Add(intptr_t(fbo.get()));
            }
            hc.Add(vertex_attributes.GetHashCode());
            hc.Add(int(cull_mode));
            hc.Add(depth_test);
            hc.Add(depth_write);

            return hc;
        }
    };

    RendererPipeline(RendererDevice *_device, ConstructionInfo &&construction_info);
    void Destroy();

    void SetPrimitive(VkPrimitiveTopology _primitive);
    void SetDynamicStates(const std::vector<VkDynamicState> &_states);

    RendererResult CreateCommandPool();
    RendererResult CreateCommandBuffers(uint16_t count);

    void UpdateDynamicStates(VkCommandBuffer cmd);
    void SetViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f);
    void SetScissor(int x, int y, uint32_t width, uint32_t height);
    void SetVertexInputMode(std::vector<VkVertexInputBindingDescription> &binding_descs, std::vector<VkVertexInputAttributeDescription> &vertex_attribs);

    inline void Build(RendererDescriptorPool *descriptor_pool)
        { Rebuild(descriptor_pool); }

    inline void Build(ConstructionInfo &&construction_info, RendererDescriptorPool *descriptor_pool)
    {
        m_construction_info = std::move(construction_info);
        
        Build(descriptor_pool);
    }

    //RendererResult CreateRenderPass(VkSampleCountFlagBits sample_count=VK_SAMPLE_COUNT_1_BIT);
    // void DoRenderPass(void (*render_callback)(RendererPipeline *pl, VkCommandBuffer *cmd));
    void StartRenderPass(VkCommandBuffer cmd, size_t index);
    void EndRenderPass(VkCommandBuffer cmd, size_t index);

    VkPrimitiveTopology GetPrimitive();
    std::vector<VkDynamicState> GetDynamicStates();

    inline const ConstructionInfo &GetConstructionInfo() const { return m_construction_info; }

    helpers::SingleTimeCommands GetSingleTimeCommands();

    /* Per frame data */
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    struct PushConstants {
        unsigned char data[128];
    } push_constants;

private:
    void Rebuild(RendererDescriptorPool *descriptor_pool);

    VkBuffer *intern_vertex_buffers = nullptr;
    uint64_t  intern_vertex_buffers_size = 0;

    std::vector<VkDynamicState> dynamic_states;

    VkViewport viewport;
    VkRect2D scissor;
    VkPrimitiveTopology primitive;

    std::vector<VkVertexInputBindingDescription>   vertex_binding_descriptions = { };
    std::vector<VkVertexInputAttributeDescription> vertex_attributes = { };

    RendererDevice *device;

    ConstructionInfo m_construction_info;

    std::vector<VkVertexInputAttributeDescription> BuildVertexAttributes(const RendererMeshInputAttributeSet &attribute_set);
    void SetVertexBuffers(std::vector<RendererVertexBuffer> &vertex_buffers);
};

}; // namespace hyperion

#endif //HYPERION_RENDERER_PIPELINE_H
