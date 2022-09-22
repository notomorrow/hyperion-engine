#ifndef HYPERION_V2_ENVIRONMENT_H
#define HYPERION_V2_ENVIRONMENT_H

#include <rendering/Shadows.hpp>
#include <core/Base.hpp>
#include <rendering/Light.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/ParticleSystem.hpp>

#include <core/Containers.hpp>
#include <core/lib/ComponentSet.hpp>
#include <core/lib/AtomicLock.hpp>
#include <core/lib/Queue.hpp>
#include <core/lib/Pair.hpp>
#include <Types.hpp>

#include <vector>

namespace hyperion::renderer {

class Frame;

} // namespace hyperion::renderer

namespace hyperion::v2 {

class Engine;
class Scene;

using RenderEnvironmentUpdates = UInt8;

enum RenderEnvironmentUpdateBits : RenderEnvironmentUpdates {
    RENDER_ENVIRONMENT_UPDATES_NONE              = 0x0,
    RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS = 0x1,
    RENDER_ENVIRONMENT_UPDATES_LIGHTS            = 0x2,
    RENDER_ENVIRONMENT_UPDATES_ENTITIES          = 0x4,
    RENDER_ENVIRONMENT_UPDATES_ENV_PROBES        = 0x8
};

class RenderEnvironment
    : public EngineComponentBase<STUB_CLASS(RenderEnvironment)>
{
public:
    RenderEnvironment(Scene *scene);
    RenderEnvironment(const RenderEnvironment &other) = delete;
    RenderEnvironment &operator=(const RenderEnvironment &other) = delete;
    ~RenderEnvironment();

    Scene *GetScene() const { return m_scene; }

    void AddLight(Handle<Light> &&light);
    void RemoveLight(Handle<Light> &&light);
    // Call from render thread only!
    SizeType NumLights() const { return m_lights.Size(); }

    void AddEnvProbe(Ref<EnvProbe> &&env_probe);
    void RemoveEnvProbe(Ref<EnvProbe> &&env_probe);
    // Call from render thread only!
    SizeType NumEnvProbes() const { return m_env_probes.Size(); }

    Handle<ParticleSystem> &GetParticleSystem()
        { return m_particle_system; }

    const Handle<ParticleSystem> &GetParticleSystem() const
        { return m_particle_system; }

    template <class T>
    void AddRenderComponent(UniquePtr<T> &&component)
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        AssertThrow(component != nullptr);

        m_render_component_sp.Wait();

        component->SetParent(this);
        m_render_components_pending_addition.Set<T>(std::move(component));

        m_render_component_sp.Signal();
        
        m_update_marker |= RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS;
    }

    template <class T, class ...Args>
    void AddRenderComponent(Args &&... args)
    {
        AddRenderComponent(UniquePtr<T>::Construct(std::forward<Args>(args)...));
    }

    /*! CALL FROM RENDER THREAD ONLY */
    template <class T>
    T *GetRenderComponent()
    {
        Threads::AssertOnThread(THREAD_RENDER);

        if (!m_render_components.Contains<T>()) {
            return nullptr;
        }

        return static_cast<T *>(m_render_components.At<T>().Get());
    }

    /*! CALL FROM RENDER THREAD ONLY */
    template <class T>
    bool HasRenderComponent() const
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        Threads::AssertOnThread(THREAD_RENDER);

        return m_render_components.Contains<T>();
    }

    template <class T>
    void RemoveRenderComponent()
    {
        static_assert(std::is_base_of_v<RenderComponentBase, T>,
            "Component should be a derived class of RenderComponentBase");

        // std::lock_guard guard(m_render_component_mutex);

        m_render_component_sp.Wait();

        m_render_components_pending_removal.Insert(RenderComponentPendingRemovalEntry {
            TypeID::ForType<T>(),
            T::ComponentName
        });

        m_render_component_sp.Signal();

        m_update_marker |= RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS;
    }

    // only touch from render thread!
    UInt32 GetEnabledRenderComponentsMask() const { return m_current_enabled_render_components_mask; }

    void OnEntityAdded(Handle<Entity> &entity);
    void OnEntityRemoved(Handle<Entity> &entity);
    // only called when meaningful attributes have changed
    void OnEntityRenderableAttributesChanged(Handle<Entity> &entity);

    float GetGlobalTimer() const { return m_global_timer; }

    void Init(Engine *engine);
    void Update(Engine *engine, GameCounter::TickUnit delta);

    void RenderComponents(Engine *engine, Frame *frame);

private:
    using RenderComponentPendingRemovalEntry = Pair<TypeID, RenderComponentName>;

    Scene *m_scene;

    std::atomic<RenderEnvironmentUpdates> m_update_marker { RENDER_ENVIRONMENT_UPDATES_NONE };

    Queue<Handle<Entity>> m_entities_pending_addition;
    Queue<Handle<Entity>> m_entities_pending_removal;
    Queue<Handle<Entity>> m_entity_renderable_attribute_updates;
    BinarySemaphore m_entity_update_sp;

    TypeMap<UniquePtr<RenderComponentBase>> m_render_components; // only touch from render thread
    TypeMap<UniquePtr<RenderComponentBase>> m_render_components_pending_addition;
    FlatSet<RenderComponentPendingRemovalEntry> m_render_components_pending_removal;
    UInt32 m_current_enabled_render_components_mask;
    UInt32 m_next_enabled_render_components_mask;

    FlatMap<Light::ID, Handle<Light>> m_lights;
    Queue<Handle<Light>> m_lights_pending_addition;
    Queue<Handle<Light>> m_lights_pending_removal;
    BinarySemaphore m_light_update_sp;

    FlatMap<EnvProbe::ID, Ref<EnvProbe>> m_env_probes;
    Queue<Ref<EnvProbe>> m_env_probes_pending_addition;
    Queue<Ref<EnvProbe>> m_env_probes_pending_removal;
    BinarySemaphore m_env_probes_update_sp;

    Handle<ParticleSystem> m_particle_system;

    float m_global_timer;

    BinarySemaphore m_render_component_sp;
    AtomicLock m_updating_render_components;
};

} // namespace hyperion::v2

#endif