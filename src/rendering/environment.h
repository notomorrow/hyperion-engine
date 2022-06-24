#ifndef HYPERION_V2_ENVIRONMENT_H
#define HYPERION_V2_ENVIRONMENT_H

#include "base.h"
#include <rendering/render_components/shadows.h>
#include "light.h"

#include <core/lib/component_set.h>
#include <core/lib/atomic_lock.h>
#include <types.h>

#include <mutex>
#include <vector>

namespace hyperion::renderer {

class Frame;

} // namespace hyperion::renderer

namespace hyperion::v2 {

class Engine;
class Scene;

class Environment : public EngineComponentBase<STUB_CLASS(Environment)> {
public:
    static constexpr UInt max_shadow_maps = 1; /* tmp */

    Environment(Scene *scene);
    Environment(const Environment &other) = delete;
    Environment &operator=(const Environment &other) = delete;
    ~Environment();

    Scene *GetScene() const                               { return m_scene; }
    
    Ref<Light> &GetLight(size_t index)                    { return m_lights[index]; }
    const Ref<Light> &GetLight(size_t index) const        { return m_lights[index]; }
    void AddLight(Ref<Light> &&light);
    size_t NumLights() const                              { return m_lights.size(); }
    const std::vector<Ref<Light>> &GetLights() const      { return m_lights; }

    template <class T>
    void AddRenderComponent(std::unique_ptr<T> &&component)
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        AssertThrow(component != nullptr);

        std::lock_guard guard(m_render_component_mutex);

        component->SetParent(this);
        m_render_components_pending_addition.Set<T>(std::move(component));

        m_has_render_component_updates = true;
    }

    template <class T, class ...Args>
    void AddRenderComponent(Args &&... args)
    {
        AddRenderComponent(std::make_unique<T>(std::forward<Args>(args)...));
    }

    /*! CALL FROM RENDER THREAD ONLY */
    template <class T>
    T *GetRenderComponent()
    {
        Threads::AssertOnThread(THREAD_RENDER);

        if (!m_render_components.Has<T>()) {
            return nullptr;
        }

        return m_render_components.Get<T>();
    }

    /*! CALL FROM RENDER THREAD ONLY */
    template <class T>
    bool HasRenderComponent() const
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        Threads::AssertOnThread(THREAD_RENDER);

        return m_render_components.Has<T>();
    }

    template <class T>
    void RemoveRenderComponent()
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        std::lock_guard guard(m_render_component_mutex);

        m_render_components_pending_removal.Insert(decltype(m_render_components)::GetComponentId<T>());

        m_has_render_component_updates = true;
    }

    void OnEntityAdded(Ref<Spatial> &entity)
    {
        Threads::AssertOnThread(THREAD_GAME);

        m_spatials_pending_addition.push(entity.IncRef());

        m_has_render_component_updates = true;
    }

    void OnEntityRemoved(Ref<Spatial> &entity)
    {
        Threads::AssertOnThread(THREAD_GAME);

        m_spatials_pending_removal.push(entity.IncRef());

        m_has_render_component_updates = true;
    }

    // only called when meaningful attributes have changed
    void OnEntityRenderableAttributesChanged(Ref<Spatial> &entity)
    {
        Threads::AssertOnThread(THREAD_GAME);

        m_spatial_renderable_attribute_updates.push(entity.IncRef());

        m_has_render_component_updates = true;
    }

    float GetGlobalTimer() const { return m_global_timer; }

    void Init(Engine *engine);
    void Update(Engine *engine, GameCounter::TickUnit delta);

    void RenderComponents(Engine *engine, Frame *frame);

private:
    Scene *m_scene;

    std::queue<Ref<Spatial>>                                     m_spatials_pending_addition;
    std::queue<Ref<Spatial>>                                     m_spatials_pending_removal;
    std::queue<Ref<Spatial>>                                     m_spatial_renderable_attribute_updates;
    std::atomic_bool                                             m_has_spatial_updates{false};
    std::mutex                                                   m_spatial_update_mutex;

    ComponentSetUnique<RenderComponentBase>                       m_render_components; // only touch from render thread
    ComponentSetUnique<RenderComponentBase>                       m_render_components_pending_addition;
    FlatSet<ComponentSetUnique<RenderComponentBase>::ComponentId> m_render_components_pending_removal;
    std::atomic_bool                                              m_has_render_component_updates{false};

    std::vector<Ref<Light>>                                       m_lights;

    float                                                         m_global_timer;

    std::mutex                                                    m_render_component_mutex;
    AtomicLock                                                    m_updating_render_components;
};

} // namespace hyperion::v2

#endif