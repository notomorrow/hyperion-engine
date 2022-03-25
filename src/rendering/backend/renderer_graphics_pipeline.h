//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_GRAPHICS_PIPELINE_H
#define HYPERION_RENDERER_GRAPHICS_PIPELINE_H

#include <vulkan/vulkan.h>

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_buffer.h"
#include "renderer_shader.h"
#include "renderer_render_pass.h"
#include "renderer_descriptor_set.h"
#include "renderer_command_buffer.h"
#include "renderer_helpers.h"

#include "../../hash_code.h"

namespace hyperion {
namespace renderer {
class FramebufferObject;
class GraphicsPipeline {
public:
    static constexpr uint32_t max_dynamic_textures = 8;

    enum class CullMode {
        NONE,
        BACK,
        FRONT
    };

    struct ConstructionInfo {
        MeshInputAttributeSet vertex_attributes;
        VkPrimitiveTopology topology;

        CullMode cull_mode;

        bool depth_test,
             depth_write;

        ShaderProgram *shader;
        RenderPass *render_pass;
        std::vector<FramebufferObject *> fbos;
    };

    GraphicsPipeline();
    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    inline const std::vector<VkDynamicState> &GetDynamicStates() const
        { return dynamic_states; }

    inline void SetDynamicStates(const std::vector<VkDynamicState> &states)
        { dynamic_states = states; }

    void SetViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f);
    void SetScissor(int x, int y, uint32_t width, uint32_t height);
    void SetVertexInputMode(std::vector<VkVertexInputBindingDescription> &binding_descs, std::vector<VkVertexInputAttributeDescription> &vertex_attribs);
    
    Result Create(Device *device, ConstructionInfo &&construction_info, DescriptorPool *descriptor_pool);

    Result Destroy(Device *device);

    void BeginRenderPass(CommandBuffer *cmd, size_t index, VkSubpassContents contents);
    void EndRenderPass(CommandBuffer *cmd, size_t index);
    void Bind(CommandBuffer *cmd);
    void SubmitPushConstants(CommandBuffer *cmd) const;

    inline const ConstructionInfo &GetConstructionInfo() const { return m_construction_info; }

    VkPipeline pipeline;
    VkPipelineLayout layout;

    struct PushConstantData {
        uint32_t previous_frame_index;
        uint32_t current_frame_index;
        uint32_t material_index;
    } push_constants;

private:
    Result Rebuild(Device *device, DescriptorPool *descriptor_pool);
    void UpdateDynamicStates(VkCommandBuffer cmd);

    std::vector<VkDynamicState> dynamic_states;

    VkViewport viewport;
    VkRect2D scissor;

    std::vector<VkVertexInputBindingDescription>   vertex_binding_descriptions{};
    std::vector<VkVertexInputAttributeDescription> vertex_attributes{};

    ConstructionInfo m_construction_info;

    std::vector<VkVertexInputAttributeDescription> BuildVertexAttributes(const MeshInputAttributeSet &attribute_set);
};

} // namespace renderer
}; // namespace hyperion

#endif //HYPERION_RENDERER_GRAPHICS_PIPELINE_H
