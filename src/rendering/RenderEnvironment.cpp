#include "RenderEnvironment.hpp"
#include <Engine.hpp>

#include <rendering/backend/RendererFrame.hpp>

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(RemoveAllRenderComponents) : RenderCommand
{
    TypeMap<UniquePtr<RenderComponentBase>> *render_components;

    RENDER_COMMAND(RemoveAllRenderComponents)(TypeMap<UniquePtr<RenderComponentBase>> *render_components)
        : render_components(render_components)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        for (auto &it : *render_components) {
            if (it.second == nullptr) {
                result = { Result::RENDERER_ERR, "RenderComponent was null" };

                continue;
            }

            it.second->ComponentRemoved();
        }

        render_components->Clear();

        return result;
    }
};

RenderEnvironment::RenderEnvironment(Scene *scene)
    : EngineComponentBase(),
      m_scene(scene),
      m_global_timer(0.0f),
      m_frame_counter(0),
      m_current_enabled_render_components_mask(0),
      m_next_enabled_render_components_mask(0),
      m_rt_radiance(Extent2D { 1024, 1024 }),
      m_probe_system({
          .aabb = {{-300.0f, -10.0f, -300.0f}, {300.0f, 400.0f, 300.0f}}
      }),
      m_has_rt_radiance(false)
{
}

RenderEnvironment::~RenderEnvironment()
{
    Teardown();
}

void RenderEnvironment::SetTLAS(const Handle<TLAS> &tlas)
{
    m_tlas = tlas;

    InitObject(m_tlas);

    m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_TLAS);
}

void RenderEnvironment::SetTLAS(Handle<TLAS> &&tlas)
{
    m_tlas = std::move(tlas);

    InitObject(m_tlas);

    m_update_marker.fetch_or(RENDER_ENVIRONMENT_UPDATES_TLAS);
}

void RenderEnvironment::Init()
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init();

    m_particle_system = CreateObject<ParticleSystem>();
    InitObject(m_particle_system);
    
    if (m_tlas) {
        m_rt_radiance.SetTLAS(m_tlas);
        m_rt_radiance.Create();

        m_probe_system.SetTLAS(m_tlas);
        m_probe_system.Init();

        m_has_rt_radiance = true;
    }

    {
        std::lock_guard guard(m_render_component_mutex);
        
        for (auto &it : m_render_components_pending_addition) {
            AssertThrow(it.second != nullptr);
            it.second->ComponentInit();
        }
    }

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        m_particle_system.Reset();
            
        if (Engine::Get()->GetConfig().Get(CONFIG_RT_SUPPORTED) && m_has_rt_radiance) {
            m_rt_radiance.Destroy();
            m_probe_system.Destroy();

            m_has_rt_radiance = false;
        }

        const auto update_marker_value = m_update_marker.load();

        RenderCommands::Push<RENDER_COMMAND(RemoveAllRenderComponents)>(&m_render_components);

        if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
            std::lock_guard guard(m_render_component_mutex);

            m_render_components_pending_addition.Clear();
            m_render_components_pending_removal.Clear();
        }

        m_update_marker.fetch_and(~RENDER_ENVIRONMENT_UPDATES_CONTAINERS);

        HYP_SYNC_RENDER();
    });
}

void RenderEnvironment::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_global_timer += delta;

    std::lock_guard guard(m_render_component_mutex);

    for (const auto &component : m_render_components) {
        component.second->ComponentUpdate(delta);
    }
}

void RenderEnvironment::ApplyTLASUpdates(Frame *frame, RTUpdateStateFlags flags)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    AssertThrow(Engine::Get()->GetConfig().Get(CONFIG_RT_SUPPORTED));
    
    if (m_has_rt_radiance) {
        m_rt_radiance.ApplyTLASUpdates(flags);
        m_probe_system.ApplyTLASUpdates(flags);
    }
}

void RenderEnvironment::RenderRTRadiance(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    AssertThrow(Engine::Get()->GetConfig().Get(CONFIG_RT_SUPPORTED));
    
    if (m_has_rt_radiance) {
        m_rt_radiance.Render(frame);

        m_probe_system.RenderProbes(frame);
        m_probe_system.ComputeIrradiance(frame);

        if (Engine::Get()->GetConfig().Get(CONFIG_RT_GI_DEBUG_PROBES)) {
            for (const auto &probe : m_probe_system.GetProbes()) {
                Engine::Get()->GetImmediateMode().Sphere(probe.position);
            }
        }
    }
}

void RenderEnvironment::RenderComponents(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    m_current_enabled_render_components_mask = m_next_enabled_render_components_mask;

    const auto update_marker_value = m_update_marker.load();
    UInt8 inverse_mask = 0;

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_ENTITIES) {
        inverse_mask |= RENDER_ENVIRONMENT_UPDATES_ENTITIES;
    }

    for (const auto &component : m_render_components) {
        m_next_enabled_render_components_mask |= 1u << static_cast<UInt32>(component.second->GetName());
        component.second->ComponentRender(frame);
    }

    // add RenderComponents AFTER rendering, so that the render scheduler can complete
    // initialization tasks before we call ComponentRender() on the newly added RenderComponents

    if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS) {
        inverse_mask |= RENDER_ENVIRONMENT_UPDATES_RENDER_COMPONENTS;

        std::lock_guard guard(m_render_component_mutex);

        for (auto &it : m_render_components_pending_addition) {
            m_render_components.Set(it.first, std::move(it.second));
        }

        m_render_components_pending_addition.Clear();

        for (const auto &it : m_render_components_pending_removal) {
            const auto components_it = m_render_components.Find(it.first);

            if (components_it != m_render_components.End()) {
                components_it->second->ComponentRemoved();
                m_next_enabled_render_components_mask &= ~(1u << static_cast<UInt32>(it.second));
                m_render_components.Erase(components_it);
            }
        }

        m_render_components_pending_removal.Clear();
    }

    if (Engine::Get()->GetConfig().Get(CONFIG_RT_ENABLED)) {
        if (update_marker_value & RENDER_ENVIRONMENT_UPDATES_TLAS) {
            inverse_mask |= RENDER_ENVIRONMENT_UPDATES_TLAS;

            if (m_tlas) {
                if (m_has_rt_radiance) {
                    m_rt_radiance.Destroy();
                    m_probe_system.Destroy();
                }
                
                m_probe_system.SetTLAS(m_tlas);
                m_rt_radiance.SetTLAS(m_tlas);

                m_rt_radiance.Create();
                m_probe_system.Init();
                m_has_rt_radiance = true;

                HYP_SYNC_RENDER();
            } else {
                if (m_has_rt_radiance) {
                    m_rt_radiance.Destroy();
                    m_probe_system.Destroy();

                    HYP_SYNC_RENDER();
                }

                m_has_rt_radiance = false;
            }
        }

        // for RT, we may need to perform resizing of buffers, and thus modification of descriptor sets
        if (const auto &tlas = m_scene->GetTLAS()) {
            RTUpdateStateFlags update_state_flags = renderer::RT_UPDATE_STATE_FLAGS_NONE;

            tlas->UpdateRender(
                frame,
                update_state_flags
            );

            if (update_state_flags) {
                ApplyTLASUpdates(frame, update_state_flags);
            }
        }
    }

    if (update_marker_value) {
        m_update_marker.fetch_and(~inverse_mask);
    }

    ++m_frame_counter;
}

} // namespace hyperion::v2
