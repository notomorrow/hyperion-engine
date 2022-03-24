#ifndef HYPERION_V2_GRAPHICS_H
#define HYPERION_V2_GRAPHICS_H

#include "material.h"
#include "shader.h"
#include "texture.h"
#include "framebuffer.h"
#include "render_pass.h"
#include "util.h"

/* TMP */
#include <rendering/mesh.h>
#include <math/transform.h>

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
    struct Spatial {
        std::shared_ptr<Mesh> mesh; /* TMP */
        MeshInputAttributeSet attributes;
        Transform transform;
        Material::ID material_id;
    };

    GraphicsPipeline(Shader::ID shader_id, RenderPass::ID render_pass_id);
    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    inline VkPrimitiveTopology GetTopology() const
        { return m_topology; }
    inline void SetTopology(VkPrimitiveTopology topology)
        { m_topology = topology; }

    void AddSpatial(Spatial &&spatial);

    /* Non-owned objects - owned by `engine`, used by the pipeline */

    inline void AddFramebuffer(Framebuffer::ID id) { m_fbo_ids.Add(id); }
    inline void RemoveFramebuffer(Framebuffer::ID id) { m_fbo_ids.Remove(id); }
    
    /* Build pipeline */
    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void Render(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index);

private:
    Shader::ID m_shader_id;
    RenderPass::ID m_render_pass_id;
    MeshInputAttributeSet m_vertex_attributes;
    VkPrimitiveTopology m_topology;

    ObjectIdHolder<Texture> m_texture_ids;
    ObjectIdHolder<Framebuffer> m_fbo_ids;

    std::vector<Spatial> m_spatials;
};

} // namespace hyperion::v2

#endif