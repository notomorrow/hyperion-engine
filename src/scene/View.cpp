/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/Light.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/camera/Camera.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/LightmapVolumeComponent.hpp>

#include <rendering/RenderView.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/lightmapper/RenderLightmapVolume.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/algorithm/Map.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

// #define HYP_DISABLE_VISIBILITY_CHECK
namespace hyperion {

#pragma region View

View::View()
    : View(ViewDesc {})
{
}

View::View(const ViewDesc& view_desc)
    : m_render_resource(nullptr),
      m_flags(view_desc.flags),
      m_viewport(view_desc.viewport),
      m_scenes(view_desc.scenes),
      m_camera(view_desc.camera),
      m_priority(view_desc.priority),
      m_entity_collection_flags(view_desc.entity_collection_flags),
      m_override_attributes(view_desc.override_attributes)
{
}

View::~View()
{
    if (m_render_resource)
    {
        FreeResource(m_render_resource);
        m_render_resource = nullptr;
    }
}

void View::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            if (m_render_resource)
            {
                FreeResource(m_render_resource);
                m_render_resource = nullptr;
            }
        }));

    AssertThrowMsg(m_camera.IsValid(), "Camera is not valid for View with ID #%u", GetID().Value());
    InitObject(m_camera);

    Array<TResourceHandle<RenderScene>> render_scenes;
    render_scenes.Reserve(m_scenes.Size());

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrowMsg(scene.IsValid(), "Scene is not valid for View with ID #%u", GetID().Value());
        InitObject(scene);

        AssertThrow(scene->IsReady());

        render_scenes.PushBack(TResourceHandle<RenderScene>(scene->GetRenderResource()));
    }

    m_render_resource = AllocateResource<RenderView>(this);
    m_render_resource->SetScenes(render_scenes);
    m_render_resource->SetCamera(TResourceHandle<RenderCamera>(m_camera->GetRenderResource()));
    m_render_resource->SetViewport(m_viewport);
    m_render_resource->GetRenderCollector().SetOverrideAttributes(m_override_attributes);

    SetReady(true);
}

bool View::TestRay(const Ray& ray, RayTestResults& out_results, bool use_bvh) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    bool has_hits = false;

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());

        if (scene->GetOctree().TestRay(ray, out_results, use_bvh))
        {
            has_hits = true;
        }
    }

    return has_hits;
}

void View::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    // HYP_LOG(Scene, Debug, "View #{} Update : Has {} scenes: {}",
    //     GetID().Value(),
    //     m_scenes.Size(),
    //     String::Join(Map(m_scenes, [](const Handle<Scene>& scene)
    //                      {
    //                          return HYP_FORMAT("(Name: {}, Nodes: {})", scene->GetName(), scene->GetRoot()->GetDescendants().Size());
    //                      }),
    //         ", "));

    for (const Handle<Scene>& scene : m_scenes)
    {
        scene->GetOctree().CalculateVisibility(m_camera);
    }

    CollectLights();
    CollectLightmapVolumes();
    m_last_collection_result = CollectEntities();
}

void View::SetViewport(const Viewport& viewport)
{
    m_viewport = viewport;

    if (IsInitCalled())
    {
        m_render_resource->SetViewport(viewport);
    }
}

void View::SetPriority(int priority)
{
    m_priority = priority;

    if (IsInitCalled())
    {
        m_render_resource->SetPriority(priority);
    }
}

void View::AddScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (m_scenes.Contains(scene))
    {
        return;
    }

    m_scenes.PushBack(scene);

    if (IsInitCalled())
    {
        InitObject(scene);

        m_render_resource->AddScene(TResourceHandle<RenderScene>(scene->GetRenderResource()));
    }
}

void View::RemoveScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (!m_scenes.Contains(scene))
    {
        return;
    }

    if (IsInitCalled())
    {
        m_render_resource->RemoveScene(&scene->GetRenderResource());
    }

    m_scenes.Erase(scene);
}

typename RenderProxyTracker::Diff View::CollectEntities()
{
    AssertReady();

    switch (uint32(m_entity_collection_flags & ViewEntityCollectionFlags::COLLECT_ALL))
    {
    case uint32(ViewEntityCollectionFlags::COLLECT_ALL):
        return CollectAllEntities();
    case uint32(ViewEntityCollectionFlags::COLLECT_DYNAMIC):
        return CollectDynamicEntities();
    case uint32(ViewEntityCollectionFlags::COLLECT_STATIC):
        return CollectStaticEntities();
    default:
        HYP_UNREACHABLE();
    }
}

typename RenderProxyTracker::Diff View::CollectAllEntities()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with ID #%u, cannot collect entities!", GetID().Value());

        return m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        if (scene->GetFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene \"{}\" has DETACHED flag set, cannot collect entities for render collector!", scene->GetName());

            continue;
        }

        const bool skip_frustum_culling = (m_entity_collection_flags & ViewEntityCollectionFlags::SKIP_FRUSTUM_CULLING);

        const ID<Camera> camera_id = m_camera->GetID();

        const VisibilityStateSnapshot visibility_state_snapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(camera_id);

        uint32 num_collected_entities = 0;
        uint32 num_skipped_entities = 0;

        for (auto [entity_id, mesh_component, visibility_state_component] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
            {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                if (!visibility_state_component.visibility_state)
                {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                    ++num_skipped_entities;
#endif
                    continue;
                }

                if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot))
                {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                    ++num_skipped_entities;
#endif

                    continue;
                }
#endif
            }

            ++num_collected_entities;

            AssertDebug(mesh_component.proxy != nullptr);

            AssertDebug(mesh_component.proxy->entity.IsValid());
            AssertDebug(mesh_component.proxy->mesh.IsValid());
            AssertDebug(mesh_component.proxy->material.IsValid());

            m_render_proxy_tracker.Track(entity_id, *mesh_component.proxy);
        }

#ifdef HYP_VISIBILITY_CHECK_DEBUG
        HYP_LOG(Scene, Debug, "Collected {} entities for camera {}, {} skipped", num_collected_entities, camera->GetName(), num_skipped_entities);
#endif
    }

    return m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
}

typename RenderProxyTracker::Diff View::CollectDynamicEntities()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertReady();

    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with ID #%u, cannot collect dynamic entities!", GetID().Value());
        // if camera is invalid, update without adding any entities
        return m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        if (scene->GetFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene \"{}\" has DETACHED flag set, cannot collect entities for render collector!", scene->GetName());

            continue;
        }

        const bool skip_frustum_culling = (m_entity_collection_flags & ViewEntityCollectionFlags::SKIP_FRUSTUM_CULLING);

        const ID<Camera> camera_id = m_camera->GetID();

        const VisibilityStateSnapshot visibility_state_snapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(camera_id);

        for (auto [entity_id, mesh_component, visibility_state_component, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::DYNAMIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
            {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                if (!visibility_state_component.visibility_state)
                {
                    continue;
                }

                if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot))
                {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                    HYP_LOG(Scene, Debug, "Skipping entity #{} for camera #{} due to visibility state being invalid.",
                        entity_id.Value(), camera_id.Value());
#endif

                    continue;
                }
#endif
            }

            AssertDebug(mesh_component.proxy != nullptr);

            AssertDebug(mesh_component.proxy->entity.IsValid());
            AssertDebug(mesh_component.proxy->mesh.IsValid());
            AssertDebug(mesh_component.proxy->material.IsValid());

            m_render_proxy_tracker.Track(entity_id, *mesh_component.proxy);
        }
    }

    return m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
}

typename RenderProxyTracker::Diff View::CollectStaticEntities()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertReady();

    if (!m_camera.IsValid())
    {
        // if camera is invalid, update without adding any entities
        HYP_LOG(Scene, Warning, "Camera is not valid for View with ID #%u, cannot collect static entities!", GetID().Value());
        return m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        if (scene->GetFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene has DETACHED flag set, cannot collect entities for render collector!");

            return {};
        }

        const bool skip_frustum_culling = (m_entity_collection_flags & ViewEntityCollectionFlags::SKIP_FRUSTUM_CULLING);

        const ID<Camera> camera_id = m_camera->GetID();

        const VisibilityStateSnapshot visibility_state_snapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(camera_id);

        for (auto [entity_id, mesh_component, visibility_state_component, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::STATIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
            {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                if (!visibility_state_component.visibility_state)
                {
                    continue;
                }

                if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot))
                {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                    HYP_LOG(Scene, Debug, "Skipping entity #{} for camera #{} due to visibility state being invalid.",
                        entity_id.Value(), camera_id.Value());
#endif

                    continue;
                }
#endif
            }

            AssertDebug(mesh_component.proxy != nullptr);

            AssertDebug(mesh_component.proxy->entity.IsValid());
            AssertDebug(mesh_component.proxy->mesh.IsValid());
            AssertDebug(mesh_component.proxy->material.IsValid());

            m_render_proxy_tracker.Track(entity_id, *mesh_component.proxy);
        }
    }

    return m_render_resource->UpdateTrackedRenderProxies(m_render_proxy_tracker);
}

void View::CollectLights()
{
    HYP_SCOPE;

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        for (auto [entity_id, light_component, transform_component, visibility_state_component] : scene->GetEntityManager()->GetEntitySet<LightComponent, TransformComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!light_component.light.IsValid())
            {
                continue;
            }

            bool is_light_in_frustum = false;

            if (visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)
            {
                is_light_in_frustum = true;
            }
            else
            {
                switch (light_component.light->GetLightType())
                {
                case LightType::DIRECTIONAL:
                    is_light_in_frustum = true;
                    break;
                case LightType::POINT:
                    is_light_in_frustum = m_camera->GetFrustum().ContainsBoundingSphere(light_component.light->GetBoundingSphere());
                    break;
                case LightType::SPOT:
                    // @TODO Implement frustum culling for spot lights
                    is_light_in_frustum = true;
                    break;
                case LightType::AREA_RECT:
                    is_light_in_frustum = m_camera->GetFrustum().ContainsAABB(light_component.light->GetAABB());
                    break;
                default:
                    break;
                }
            }

            if (is_light_in_frustum)
            {
                m_tracked_lights.Track(light_component.light->GetID(), &light_component.light->GetRenderResource());
            }
        }
    }

    m_render_resource->UpdateTrackedLights(m_tracked_lights);
}

void View::CollectLightmapVolumes()
{
    HYP_SCOPE;

    for (const Handle<Scene>& scene : m_scenes)
    {
        AssertThrow(scene.IsValid());
        AssertThrow(scene->IsReady());

        for (auto [entity_id, lightmap_volume_component] : scene->GetEntityManager()->GetEntitySet<LightmapVolumeComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!lightmap_volume_component.volume.IsValid())
            {
                continue;
            }

            const BoundingBox& volume_aabb = lightmap_volume_component.volume->GetAABB();

            if (!volume_aabb.IsValid() || !volume_aabb.IsFinite())
            {
                HYP_LOG(Scene, Warning, "Lightmap volume {} has an invalid AABB in view {}", lightmap_volume_component.volume->GetID().Value(), GetID().Value());

                continue;
            }

            if (!m_camera->GetFrustum().ContainsAABB(volume_aabb))
            {
                continue;
            }

            ResourceTracker<ID<LightmapVolume>, RenderLightmapVolume*>::ResourceTrackState track_state;

            m_tracked_lightmap_volumes.Track(
                lightmap_volume_component.volume->GetID(),
                &lightmap_volume_component.volume->GetRenderResource(),
                &track_state);

            if (track_state & ResourceTracker<ID<LightmapVolume>, RenderLightmapVolume*>::CHANGED)
            {
                HYP_LOG(Scene, Debug, "Lightmap volume {} changed in view {}", lightmap_volume_component.volume->GetID().Value(), GetID().Value());
            }
        }
    }

    m_render_resource->UpdateTrackedLightmapVolumes(m_tracked_lightmap_volumes);
}

#pragma endregion View

} // namespace hyperion
