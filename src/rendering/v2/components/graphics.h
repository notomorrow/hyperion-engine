#ifndef HYPERION_V2_GRAPHICS_H
#define HYPERION_V2_GRAPHICS_H

#include "shader.h"
#include "spatial.h"
#include "texture.h"
#include "framebuffer.h"
#include "render_pass.h"
#include "util.h"

#include <rendering/backend/renderer_graphics_pipeline.h>

#include <memory>
#include <vector>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::DescriptorSet;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::MeshInputAttributeSet;

class Engine;

class GraphicsPipeline : public EngineComponent<renderer::GraphicsPipeline> {
public:

    GraphicsPipeline(Shader::ID shader_id, RenderPass::ID render_pass_id);
    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    inline Shader::ID GetShaderID() const { return m_shader_id; }
    inline RenderPass::ID GetRenderPassID() const { return m_render_pass_id; }
    inline const MeshInputAttributeSet &GetVertexAttributes() const { return m_vertex_attributes; }

    inline VkPrimitiveTopology GetTopology() const
        { return m_topology; }
    inline void SetTopology(VkPrimitiveTopology topology)
        { m_topology = topology; }

    void AddSpatial(Engine *engine, Spatial &&spatial);
    void SetSpatialTransform(Engine *engine, uint32_t index, const Transform &transform);

    /* Non-owned objects - owned by `engine`, used by the pipeline */

    inline void AddFramebuffer(Framebuffer::ID id) { m_fbo_ids.Add(id); }
    inline void RemoveFramebuffer(Framebuffer::ID id) { m_fbo_ids.Remove(id); }
    
    /* Build pipeline */
    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void Render(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index);
    void Render(Engine *engine, CommandBuffer *primary_command_buffer, CommandBuffer *secondary_command_buffer, uint32_t frame_index);

private:
    Shader::ID m_shader_id;
    RenderPass::ID m_render_pass_id;
    VkPrimitiveTopology m_topology;
    MeshInputAttributeSet m_vertex_attributes;

    ObjectIdHolder<Texture> m_texture_ids;
    ObjectIdHolder<Framebuffer> m_fbo_ids;

    std::vector<Spatial> m_spatials;
};

} // namespace hyperion::v2

#endif