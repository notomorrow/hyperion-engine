#ifndef HYPERION_V2_GRAPHICS_H
#define HYPERION_V2_GRAPHICS_H

#include "shader.h"
#include "spatial.h"
#include "scene.h"
#include "texture.h"
#include "framebuffer.h"
#include "render_pass.h"
#include "containers.h"

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
using renderer::Topology;
using renderer::FillMode;
using renderer::CullMode;

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

    GraphicsPipeline(Ref<Shader> &&shader, Ref<Scene> &&scene, Ref<RenderPass> &&render_pass, Bucket bucket);
    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    inline Shader *GetShader() const { return m_shader.ptr; }
    inline Scene *GetScene() const { return m_scene.ptr; }
    inline RenderPass *GetRenderPassID() const { return m_render_pass.ptr; }
    inline Bucket GetBucket() const { return m_bucket; }

    inline MeshInputAttributeSet &GetVertexAttributes() { return m_vertex_attributes; }
    inline const MeshInputAttributeSet &GetVertexAttributes() const { return m_vertex_attributes; }

    inline Topology GetTopology() const
        { return m_topology; }
    inline void SetTopology(Topology topology)
        { m_topology = topology; }

    inline auto GetFillMode() const
        { return m_fill_mode; }
    inline void SetFillMode(FillMode fill_mode)
        { m_fill_mode = fill_mode; }

    inline auto GetCullMode() const
        { return m_cull_mode; }
    inline void SetCullMode(CullMode cull_mode)
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

    void AddSpatial(Ref<Spatial> &&spatial);
    void RemoveSpatial(Spatial::ID id);
    inline auto &GetSpatials() { return m_spatials; }
    inline const auto &GetSpatials() const { return m_spatials; }

    /* Non-owned objects - owned by `engine`, used by the pipeline */

    inline void AddFramebuffer(Ref<Framebuffer> &&fbo)
        { m_fbos.push_back(std::move(fbo)); }

    inline auto &GetFramebuffers() { return m_fbos; } 
    inline const auto &GetFramebuffers() const { return m_fbos; }
    
    /* Build pipeline */
    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index);

private:
    /* Called from Spatial - remove the pointer */
    void OnSpatialRemoved(Spatial *spatial);

    Ref<Shader> m_shader;
    Ref<Scene> m_scene;
    Ref<RenderPass> m_render_pass;
    Bucket m_bucket;
    Topology m_topology;
    CullMode m_cull_mode;
    FillMode m_fill_mode;
    bool m_depth_test;
    bool m_depth_write;
    bool m_blend_enabled;
    MeshInputAttributeSet m_vertex_attributes;
    
    std::vector<Ref<Framebuffer>> m_fbos;

    std::vector<Ref<Spatial>> m_spatials;

    PerFrameData<CommandBuffer> *m_per_frame_data;
};

} // namespace hyperion::v2

#endif