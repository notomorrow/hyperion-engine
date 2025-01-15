/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_WORLD_HPP
#define HYPERION_RENDERING_WORLD_HPP

#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>

#include <rendering/RenderResources.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class World;
class Scene;
class CameraRenderResources;
class SceneRenderResources;

class RenderCollectorContainer
{
public:
    static constexpr uint32 max_scenes = 128;

    RenderCollectorContainer()
        : m_num_render_collectors { 0u }
    {
    }

    RenderCollectorContainer(const RenderCollectorContainer &other)                   = delete;
    RenderCollectorContainer &operator=(RenderCollectorContainer &other)              = delete;
    RenderCollectorContainer(RenderCollectorContainer &&other) noexcept               = delete;
    RenderCollectorContainer &operator=(RenderCollectorContainer &&other) noexcept    = delete;
    ~RenderCollectorContainer()                                                  = default;

    uint32 NumRenderCollectors() const
        { return m_num_render_collectors.Get(MemoryOrder::ACQUIRE); }

    void AddScene(const Scene *scene);
    void RemoveScene(ID<Scene> id);

    RenderCollector &GetRenderCollectorForScene(ID<Scene> scene_id)
        { return m_render_collectors_by_id_index[scene_id.ToIndex()]; }

    const RenderCollector &GetRenderCollectorForScene(ID<Scene> scene_id) const
        { return m_render_collectors_by_id_index[scene_id.ToIndex()]; }

    RenderCollector *GetRenderCollectorAtIndex(uint32 index) const
        { return m_render_collectors[index]; }

private:
    FixedArray<RenderCollector *, max_scenes>   m_render_collectors;
    FixedArray<RenderCollector, max_scenes>     m_render_collectors_by_id_index;
    AtomicVar<uint32>                           m_num_render_collectors;
};

class WorldRenderResources final : public RenderResourcesBase
{
public:
    WorldRenderResources(World *world);
    virtual ~WorldRenderResources() override;

    HYP_FORCE_INLINE World *GetWorld() const
        { return m_world; }

    HYP_FORCE_INLINE RenderCollectorContainer &GetRenderCollectorContainer()
        { return m_render_collector_container; }

    HYP_FORCE_INLINE const RenderCollectorContainer &GetRenderCollectorContainer() const
        { return m_render_collector_container; }

    HYP_FORCE_INLINE RenderCollector &GetRenderCollectorForScene(ID<Scene> scene_id)
        { return m_render_collector_container.GetRenderCollectorForScene(scene_id); }

    HYP_FORCE_INLINE const RenderCollector &GetRenderCollectorForScene(ID<Scene> scene_id) const
        { return m_render_collector_container.GetRenderCollectorForScene(scene_id); }

    void AddScene(const Handle<Scene> &scene);
    void RemoveScene(const WeakHandle<Scene> &scene_weak);

    void PreRender(renderer::Frame *frame);
    void PostRender(renderer::Frame *frame);
    void Render(renderer::Frame *frame);

protected:
    virtual void Initialize() override;
    virtual void Destroy() override;
    virtual void Update() override;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

    virtual Name GetTypeName() const override
        { return NAME("WorldRenderResources"); }

private:
    World                                           *m_world;
    Array<TResourceHandle<CameraRenderResources>>   m_bound_cameras;
    Array<TResourceHandle<SceneRenderResources>>    m_bound_scenes;
    RenderCollectorContainer                        m_render_collector_container;
};

} // namespace hyperion

#endif
