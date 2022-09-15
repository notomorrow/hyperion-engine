#ifndef HYPERION_V2_RENDERER_H
#define HYPERION_V2_RENDERER_H

#include <core/Containers.hpp>
#include <animation/Skeleton.hpp>
#include <scene/Entity.hpp>
#include "Shader.hpp"
#include "Framebuffer.hpp"
#include "RenderPass.hpp"
#include "RenderBucket.hpp"
#include "RenderableAttributes.hpp"
#include "IndirectDraw.hpp"
#include "CullData.hpp"
#include "Compute.hpp"
#include <Constants.hpp>
#include <core/lib/AtomicSemaphore.hpp>
#include <util/Defines.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFrame.hpp>

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
using renderer::FaceCullMode;
using renderer::StencilState;
using renderer::Frame;
using renderer::Pipeline;
using renderer::Extent3D;
using renderer::StorageBuffer;

class Engine;

class RendererInstance
    : public EngineComponentBase<STUB_CLASS(RendererInstance)>
{
    friend class Engine;
    friend class Entity;

public:
    static constexpr bool parallel_rendering = HYP_FEATURES_PARALLEL_RENDERING;

    RendererInstance(
        Handle<Shader> &&shader,
        Handle<RenderPass> &&render_pass,
        const RenderableAttributeSet &renderable_attributes
    );

    RendererInstance(const RendererInstance &other) = delete;
    RendererInstance &operator=(const RendererInstance &other) = delete;
    ~RendererInstance();

    renderer::GraphicsPipeline *GetPipeline() const { return m_pipeline.get(); }

    Handle<Shader> &GetShader() { return m_shader; }
    const Handle<Shader> &GetShader() const { return m_shader; }
    
    const RenderableAttributeSet &GetRenderableAttributes() const { return m_renderable_attributes; }
    
    void AddEntity(Handle<Entity> &&entity);
    void RemoveEntity(Handle<Entity> &&entity, bool call_on_removed = true);
    auto &GetEntities() { return m_entities; }
    const auto &GetEntities() const { return m_entities; }

    void AddFramebuffer(Handle<Framebuffer> &&fbo) { m_fbos.PushBack(std::move(fbo)); }
    void RemoveFramebuffer(Framebuffer::ID id);
    DynArray<Handle<Framebuffer>> &GetFramebuffers() { return m_fbos; } 
    const DynArray<Handle<Framebuffer>> &GetFramebuffers() const { return m_fbos; }
    
    void Init(Engine *engine);

    /*! \brief Collect drawable objects, must only be used with non-indirect rendering!
     */
    void CollectDrawCalls(
        Engine *engine,
        Frame *frame
    );

    /*! \brief Collect drawable objects, then run the culling compute shader
     * to mark any occluded objects as such. Must be used with indirect rendering.
     */
    void CollectDrawCalls(
        Engine *engine,
        Frame *frame,
        const CullData &cull_data
    );

    /*! \brief Render objects using direct rendering, no occlusion culling is provided. */
    void PerformRendering(
        Engine *engine,
        Frame *frame
    );
    
    /*! \brief Render objects using indirect rendering. The objects must have had the culling shader ran on them,
     * using CollectDrawCalls(). */
    void PerformRenderingIndirect(
        Engine *engine,
        Frame *frame
    );

    // render non-indirect (collects draw calls, then renders)
    void Render(
        Engine *engine,
        Frame *frame
    );

private:
    void PerformEnqueuedEntityUpdates(Engine *engine, UInt frame_index);
    
    void UpdateEnqueuedEntitiesFlag()
    {
        m_enqueued_entities_flag.store(
           m_entities_pending_addition.Any() || m_entities_pending_removal.Any()
        );
    }

    std::unique_ptr<renderer::GraphicsPipeline> m_pipeline;

    Handle<Shader> m_shader;
    Handle<RenderPass> m_render_pass;
    RenderableAttributeSet m_renderable_attributes;

    IndirectRenderer m_indirect_renderer;
    
    DynArray<Handle<Framebuffer>> m_fbos;

    DynArray<Handle<Entity>> m_entities; // lives in RENDER thread
    DynArray<Handle<Entity>> m_entities_pending_addition; // shared
    DynArray<Handle<Entity>> m_entities_pending_removal; // shared

    // for each frame in flight - have an array of command buffers to use
    // for async command buffer recording.
    FixedArray<FixedArray<UniquePtr<CommandBuffer>, num_async_rendering_command_buffers>, max_frames_in_flight> m_command_buffers;

    // std::mutex m_enqueued_entities_mutex;
    BinarySemaphore m_enqueued_entities_sp;
    std::atomic_bool m_enqueued_entities_flag { false };

    // cycle through command buffers, so you can call Render()
    // multiple times in a single pass, only running into issues if you
    // try to call it more than num_async_rendering_command_buffers
    // (or if parallel rendering is enabled, more than the number of task threads available (usually 2))
    UInt m_command_buffer_index = 0u;
};

} // namespace hyperion::v2

#endif
