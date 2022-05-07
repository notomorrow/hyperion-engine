#ifndef HYPERION_V2_GRAPHICS_H
#define HYPERION_V2_GRAPHICS_H

#include "shader.h"
#include "../scene/spatial.h"
#include "../scene/scene.h"
#include "texture.h"
#include "framebuffer.h"
#include "render_pass.h"
#include "containers.h"
#include "render_bucket.h"

#include <rendering/backend/renderer_graphics_pipeline.h>

#include <memory>
#include <mutex>
#include <vector>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::DescriptorSet;
using renderer::DescriptorSetBinding;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::VertexAttributeSet;
using renderer::PerFrameData;
using renderer::Topology;
using renderer::FillMode;
using renderer::CullMode;

class Engine;

class GraphicsPipeline : public EngineComponent<renderer::GraphicsPipeline> {
    friend class Engine;
    friend class Spatial;

public:
    GraphicsPipeline(
        Ref<Shader> &&shader,
        Ref<RenderPass> &&render_pass,
        const VertexAttributeSet &vertex_attributes,
        Bucket bucket
    );

    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    inline Shader *GetShader() const { return m_shader.ptr; }
    inline RenderPass *GetRenderPassID() const { return m_render_pass.ptr; }
    inline Bucket GetBucket() const { return m_bucket; }

    inline VertexAttributeSet &GetVertexAttributes() { return m_vertex_attributes; }
    inline const VertexAttributeSet &GetVertexAttributes() const { return m_vertex_attributes; }

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
    inline auto &GetSpatials()             { return m_spatials; }
    inline const auto &GetSpatials() const { return m_spatials; }

    /* Non-owned objects - owned by `engine`, used by the pipeline */

    inline void AddFramebuffer(Ref<Framebuffer> &&fbo)
        { m_fbos.push_back(std::move(fbo)); }

    inline auto &GetFramebuffers()             { return m_fbos; } 
    inline const auto &GetFramebuffers() const { return m_fbos; }
    
    /* Build pipeline */
    void Init(Engine *engine);
    void Render(
        Engine *engine,
        CommandBuffer *primary,
        uint32_t frame_index
    );

private:
    static bool BucketSupportsCulling(Bucket bucket);


    bool RemoveFromSpatialList(Spatial::ID id, std::vector<Ref<Spatial>> &spatials, bool call_on_removed, bool remove_immediately);

    /* Called from Spatial - remove the pointer */
    void OnSpatialRemoved(Spatial *spatial);

    void PerformEnqueuedSpatialUpdates(Engine *engine);

    Ref<Shader> m_shader;
    Ref<RenderPass> m_render_pass;
    Bucket m_bucket;
    Topology m_topology;
    CullMode m_cull_mode;
    FillMode m_fill_mode;
    bool m_depth_test;
    bool m_depth_write;
    bool m_blend_enabled;
    VertexAttributeSet m_vertex_attributes;
    
    std::vector<Ref<Framebuffer>> m_fbos;

    std::vector<Ref<Spatial>> m_spatials;
    std::vector<Ref<Spatial>> m_spatials_pending_addition;
    std::vector<Ref<Spatial>> m_spatials_pending_removal;

    PerFrameData<CommandBuffer> *m_per_frame_data;

    std::mutex m_enqueued_spatials_mutex;
};

} // namespace hyperion::v2

#endif