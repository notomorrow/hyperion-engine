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
using renderer::PerFrameData;

class Engine;

class GraphicsPipeline : public EngineComponent<renderer::GraphicsPipeline> {
    friend class Engine;
    friend class Spatial;

public:
    enum Bucket {
        BUCKET_SWAPCHAIN = 0, /* Main swapchain */
        BUCKET_PREPASS,       /* Pre-pass / buffer items */
        /* === Scene objects === */
        BUCKET_OPAQUE,        /* Opaque items */
        BUCKET_TRANSLUCENT,   /* Transparent - rendering on top of opaque objects */
        BUCKET_PARTICLE,      /* Specialized particle bucket */
        BUCKET_SKYBOX,        /* Rendered without depth testing/writing, and rendered first */
        BUCKET_MAX
    };

    struct ID : EngineComponent::ID {
        Bucket bucket;

        inline bool operator==(const ID &other) const
            { return value == other.value && bucket == other.bucket; }

        inline bool operator<(const ID &other) const
            { return value < other.value && bucket < other.bucket; }
    };

    GraphicsPipeline(EngineCallbacks &callbacks, Shader::ID shader_id, RenderPass::ID render_pass_id, Bucket bucket);
    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    inline Shader::ID GetShaderID() const { return m_shader_id; }
    inline RenderPass::ID GetRenderPassID() const { return m_render_pass_id; }
    inline Bucket GetBucket() const { return m_bucket; }

    inline const MeshInputAttributeSet &GetVertexAttributes() const { return m_vertex_attributes; }

    inline VkPrimitiveTopology GetTopology() const
        { return m_topology; }
    inline void SetTopology(VkPrimitiveTopology topology)
        { m_topology = topology; }

    inline auto GetFillMode() const
        { return m_fill_mode; }
    inline void SetFillMode(renderer::GraphicsPipeline::FillMode fill_mode)
        { m_fill_mode = fill_mode; }

    inline auto GetCullMode() const
        { return m_cull_mode; }
    inline void SetCullMode(renderer::GraphicsPipeline::CullMode cull_mode)
        { m_cull_mode = cull_mode; }

    inline bool GetDepthTest() const
        { return m_depth_test; }
    inline void SetDepthTest(bool depth_test)
        { m_depth_test = depth_test; }

    inline bool GetDepthWrite() const
        { return m_depth_write; }
    inline void SetDepthWrite(bool depth_write)
        { m_depth_write = depth_write; }

    inline bool GetBlendEnabled() const
        { return m_blend_enabled; }
    inline void SetBlendEnabled(bool blend_enabled)
        { m_blend_enabled = blend_enabled; }

    void AddSpatial(Engine *engine, Spatial *spatial);
    void RemoveSpatial(Engine *engine, Spatial *spatial);
    inline const auto &GetSpatials() const { return m_spatials; }

    /* Non-owned objects - owned by `engine`, used by the pipeline */

    inline void AddFramebuffer(Framebuffer::ID id) { m_fbo_ids.Add(id); }
    inline void RemoveFramebuffer(Framebuffer::ID id) { m_fbo_ids.Remove(id); }

    inline uint32_t GetSceneIndex() const { return m_scene_index; }
    inline void SetSceneIndex(uint32_t scene_index) { m_scene_index = scene_index; }
    
    /* Build pipeline */
    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index);

private:
    /* Called from Spatial - remove the pointer */
    void OnSpatialRemoved(Spatial *spatial);

    Shader::ID m_shader_id;
    RenderPass::ID m_render_pass_id;
    Bucket m_bucket;
    VkPrimitiveTopology m_topology;
    renderer::GraphicsPipeline::CullMode m_cull_mode;
    renderer::GraphicsPipeline::FillMode m_fill_mode;
    bool m_depth_test;
    bool m_depth_write;
    bool m_blend_enabled;
    MeshInputAttributeSet m_vertex_attributes;

    ObjectIdHolder<Texture> m_texture_ids;
    ObjectIdHolder<Framebuffer> m_fbo_ids;

    std::vector<Spatial *> m_spatials;

    uint32_t m_scene_index;

    PerFrameData<CommandBuffer> *m_per_frame_data;
};

} // namespace hyperion::v2

#endif