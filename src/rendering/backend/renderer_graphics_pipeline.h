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
#include "renderer_descriptor_pool.h"
#include "renderer_descriptor_set.h"
#include "renderer_descriptor.h"
#include "renderer_command_buffer.h"
#include "renderer_helpers.h"

#include "../../hash_code.h"

namespace hyperion {
namespace renderer {
class FramebufferObject;
class GraphicsPipeline {
public:
    static constexpr uint32_t max_dynamic_textures = 8;

    struct ConstructionInfo {
        MeshInputAttributeSet vertex_attributes;
        non_owning_ptr<ShaderProgram> shader;
        uint32_t shader_id;
        non_owning_ptr<RenderPass> render_pass;
        uint32_t render_pass_id;
        std::vector<non_owning_ptr<FramebufferObject>> fbos;
        std::vector<uint32_t> fbo_ids; // unresolved fbo ids

        VkPrimitiveTopology topology;

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

            hc.Add(shader_id);
            hc.Add(render_pass_id); // TODO
            for (auto fbo_id : fbo_ids) {
                hc.Add(fbo_id);
            }
            hc.Add(vertex_attributes.GetHashCode());
            hc.Add(int(topology));
            hc.Add(int(cull_mode));
            hc.Add(depth_test);
            hc.Add(depth_write);

            return hc;
        }
    };

    class Builder {
    public:
        Builder()
        {
            Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            CullMode(ConstructionInfo::CullMode::BACK);
            DepthTest(true);
            DepthWrite(true);
        }

        Builder(Builder &&other) = default;
        Builder(const Builder &other) = delete;
        Builder &operator=(const Builder &other) = delete;
        ~Builder() = default;

        Builder &VertexAttributes(const MeshInputAttributeSet &vertex_attributes)
        {
            m_construction_info.vertex_attributes = vertex_attributes;

            return *this;
        }

        Builder &Topology(VkPrimitiveTopology topology)
        {
            m_construction_info.topology = topology;

            return *this;
        }

        Builder &CullMode(ConstructionInfo::CullMode cull_mode)
        {
            m_construction_info.cull_mode = cull_mode;

            return *this;
        }

        Builder &DepthWrite(bool depth_write)
        {
            m_construction_info.depth_write = depth_write;

            return *this;
        }

        Builder &DepthTest(bool depth_test)
        {
            m_construction_info.depth_test = depth_test;

            return *this;
        }

        template <class T>
        Builder &Shader(typename T::ID id)
        {
            m_construction_info.shader_id = id.GetValue();

            return *this;
        }

        template <class T>
        Builder &RenderPass(typename T::ID id)
        {
            m_construction_info.render_pass_id = id.GetValue();

            return *this;
        }

        template <class T>
        Builder &Framebuffer(typename T::ID id)
        {
            m_construction_info.fbo_ids.push_back(id.GetValue());

            return *this;
        }

        std::unique_ptr<GraphicsPipeline> Build()
        {
            AssertThrow(!m_construction_info.fbos.empty());

            return std::make_unique<GraphicsPipeline>(std::move(m_construction_info));
        }

        inline HashCode GetHashCode() const
        {
            return m_construction_info.GetHashCode();
        }
        
        ConstructionInfo m_construction_info;
    };

    GraphicsPipeline(ConstructionInfo &&construction_info);

    std::vector<VkDynamicState> GetDynamicStates();
    void SetDynamicStates(const std::vector<VkDynamicState> &_states);

    void SetViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f);
    void SetScissor(int x, int y, uint32_t width, uint32_t height);
    void SetVertexInputMode(std::vector<VkVertexInputBindingDescription> &binding_descs, std::vector<VkVertexInputAttributeDescription> &vertex_attribs);

    inline Result Create(Device *device, DescriptorPool *descriptor_pool)
    {
        return Rebuild(device, descriptor_pool);
    }

    inline Result Create(Device *device, ConstructionInfo &&construction_info, DescriptorPool *descriptor_pool)
    {
        m_construction_info = std::move(construction_info);

        return Create(device, descriptor_pool);
    }

    Result Destroy(Device *device);

    void BeginRenderPass(CommandBuffer *cmd, size_t index, VkSubpassContents contents);
    void EndRenderPass(CommandBuffer *cmd, size_t index);
    void Bind(CommandBuffer *cmd);

    inline const ConstructionInfo &GetConstructionInfo() const { return m_construction_info; }

    VkPipeline pipeline;
    VkPipelineLayout layout;

    struct PushConstants {
        uint32_t previous_frame_index;
        uint32_t current_frame_index;
        uint32_t texture_set_index[max_dynamic_textures];
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
