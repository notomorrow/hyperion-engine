/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <scene/ecs/EntityManager.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>

#include <scene/ecs/systems/CameraSystem.hpp>
#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/systems/RenderProxyUpdaterSystem.hpp>
#include <scene/ecs/systems/EntityMeshDirtyStateSystem.hpp>
#include <scene/ecs/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/ecs/systems/LightVisibilityUpdaterSystem.hpp>
#include <scene/ecs/systems/ShadowMapUpdaterSystem.hpp>
#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/systems/ReflectionProbeUpdaterSystem.hpp>
#include <scene/ecs/systems/AnimationSystem.hpp>
#include <scene/ecs/systems/LightmapSystem.hpp>
#include <scene/ecs/systems/SkySystem.hpp>
#include <scene/ecs/systems/AudioSystem.hpp>
#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/systems/BVHUpdaterSystem.hpp>
#include <scene/ecs/systems/PhysicsSystem.hpp>
#include <scene/ecs/systems/ScriptSystem.hpp>

#include <scene/world_grid/WorldGridSubsystem.hpp>

#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/math/Halton.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

// #define HYP_VISIBILITY_CHECK_DEBUG
// #define HYP_DISABLE_VISIBILITY_CHECK

namespace hyperion {

Scene::Scene()
    : Scene(nullptr, Handle<Camera>::empty, g_game_thread, { })
{
}
    
Scene::Scene(EnumFlags<SceneFlags> flags)
    : Scene(nullptr, Handle<Camera>::empty, g_game_thread, flags)
{
}
    
Scene::Scene(World *world, EnumFlags<SceneFlags> flags)
    : Scene(world, Handle<Camera>::empty, g_game_thread, flags)
{
}

Scene::Scene(World *world, const Handle<Camera> &camera, EnumFlags<SceneFlags> flags)
    : Scene(world, camera, g_game_thread, flags)
{
}

Scene::Scene(
    World *world,
    const Handle<Camera> &camera,
    ThreadID owner_thread_id,
    EnumFlags<SceneFlags> flags
) : m_name(Name::Unique("Scene")),
    m_flags(flags),
    m_owner_thread_id(owner_thread_id),
    m_world(world),
    m_camera(std::move(camera)),
    m_root_node_proxy(MakeRefCountedPtr<Node>("<ROOT>", Handle<Entity>::empty, Transform::identity, this)),
    m_is_audio_listener(false),
    m_entity_manager(MakeRefCountedPtr<EntityManager>(owner_thread_id.GetMask(), this)),
    m_octree(m_entity_manager, BoundingBox(Vec3f(-250.0f), Vec3f(250.0f))),
    m_previous_delta(0.01667f),
    m_render_resource(nullptr)
{
    // Disable command execution if not on game thread
    if (m_owner_thread_id != g_game_thread) {
        m_entity_manager->GetCommandQueue().SetFlags(m_entity_manager->GetCommandQueue().GetFlags() & ~EntityManagerCommandQueueFlags::EXEC_COMMANDS);
    }

    m_root_node_proxy->SetScene(this);
}

Scene::~Scene()
{
    m_octree.SetEntityManager(nullptr);
    m_octree.Clear();

    m_camera.Reset();

    if (m_root_node_proxy.IsValid()) {
        m_root_node_proxy->SetScene(nullptr);
    }

    // Move so destruction of components can check GetEntityManager() returns nullptr
    RC<EntityManager> entity_manager = std::move(m_entity_manager);
    entity_manager.Reset();

    if (m_render_resource != nullptr) {
        FreeResource(m_render_resource);

        m_render_resource = nullptr;
    }

    HYP_SYNC_RENDER();
}
    
void Scene::Init()
{
    if (IsInitCalled()) {
        return;
    }
    
    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
    {
        if (m_render_resource != nullptr) {
            FreeResource(m_render_resource);

            m_render_resource = nullptr;
        }
    }));

    m_render_resource = AllocateResource<SceneRenderResource>(this);

    InitObject(m_camera);

    AddSystemIfApplicable<CameraSystem>();
    AddSystemIfApplicable<WorldAABBUpdaterSystem>();
    AddSystemIfApplicable<EntityMeshDirtyStateSystem>();
    AddSystemIfApplicable<RenderProxyUpdaterSystem>();
    AddSystemIfApplicable<ReflectionProbeUpdaterSystem>();
    AddSystemIfApplicable<LightVisibilityUpdaterSystem>();
    AddSystemIfApplicable<ShadowMapUpdaterSystem>();
    AddSystemIfApplicable<VisibilityStateUpdaterSystem>();
    AddSystemIfApplicable<EnvGridUpdaterSystem>();
    AddSystemIfApplicable<LightmapSystem>();
    AddSystemIfApplicable<AnimationSystem>();
    AddSystemIfApplicable<SkySystem>();
    AddSystemIfApplicable<AudioSystem>();
    AddSystemIfApplicable<BLASUpdaterSystem>();
    AddSystemIfApplicable<BVHUpdaterSystem>();
    AddSystemIfApplicable<PhysicsSystem>();
    AddSystemIfApplicable<ScriptSystem>();

    m_entity_manager->Initialize();

    SetReady(true);
}

void Scene::SetOwnerThreadID(ThreadID owner_thread_id)
{
    if (m_owner_thread_id == owner_thread_id) {
        return;
    }

    m_owner_thread_id = owner_thread_id;
    m_entity_manager->SetOwnerThreadMask(owner_thread_id.GetMask());

    if (m_owner_thread_id == g_game_thread) {
        m_entity_manager->GetCommandQueue().SetFlags(m_entity_manager->GetCommandQueue().GetFlags() | EntityManagerCommandQueueFlags::EXEC_COMMANDS);
    } else {
        m_entity_manager->GetCommandQueue().SetFlags(m_entity_manager->GetCommandQueue().GetFlags() & ~EntityManagerCommandQueueFlags::EXEC_COMMANDS);
    }
}

void Scene::SetCamera(const Handle<Camera> &camera)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    m_camera = camera;

    if (IsInitCalled()) {
        InitObject(m_camera);
    }
}

void Scene::SetWorld(World *world)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    if (m_world == world) {
        return;
    }

    if (m_world != nullptr && m_world->HasScene(GetID())) {
        m_world->RemoveScene(WeakHandleFromThis());
    }

    // When world is changed, entity manager needs all systems to have this change reflected
    m_entity_manager->SetWorld(world);

    m_world = world;
}

WorldGrid *Scene::GetWorldGrid() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    if (!m_world) {
        return nullptr;
    }

    if (WorldGridSubsystem *world_grid_subsystem = m_world->GetSubsystem<WorldGridSubsystem>()) {
        return world_grid_subsystem->GetWorldGrid(GetID());
    }

    return nullptr;
}

NodeProxy Scene::FindNodeWithEntity(ID<Entity> entity) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    AssertThrow(m_root_node_proxy);

    if (m_root_node_proxy->GetEntity() == entity) {
        return m_root_node_proxy;
    }

    return m_root_node_proxy->FindChildWithEntity(entity);
}

NodeProxy Scene::FindNodeByName(UTF8StringView name) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    AssertThrow(m_root_node_proxy);

    if (m_root_node_proxy->GetName() == name) {
        return m_root_node_proxy;
    }

    return m_root_node_proxy->FindChildByName(name);
}

void Scene::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    AssertReady();

    {
        HYP_NAMED_SCOPE("Update octree");

        // Rebuild any octants that have had structural changes
        // IMPORTANT: must be ran at start of tick, as pointers to octants' visibility states will be
        // stored on VisibilityStateComponent.
        m_octree.PerformUpdates();
        m_octree.NextVisibilityState();
    }

    // if (m_camera.IsValid()) {
    //     HYP_NAMED_SCOPE("Update camera and calculate visibility");

    //     m_camera->Update(delta);

    //     m_octree.CalculateVisibility(m_camera);
    // }

    if (IsForegroundScene()) {
        m_entity_manager->FlushCommandQueue(delta);
    }
}

RenderCollector::CollectionResult Scene::CollectEntities(
    RenderCollector &render_collector,
    const Handle<Camera> &camera,
    const Optional<RenderableAttributeSet> &override_attributes,
    bool skip_frustum_culling
) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!camera.IsValid()) {
        return { };
    }

    const ID<Camera> camera_id = camera.GetID();

    const VisibilityStateSnapshot visibility_state_snapshot = m_octree.GetVisibilityState().GetSnapshot(camera_id);

    uint32 num_collected_entities = 0;
    uint32 num_skipped_entities = 0;

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component, visibility_state_component] : m_entity_manager->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT)) {
        if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)) {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
            if (!visibility_state_component.visibility_state) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                ++num_skipped_entities;
#endif
                continue;
            }

            if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                ++num_skipped_entities;
#endif

                continue;
            }
#endif
        }

        ++num_collected_entities;

        AssertThrow(mesh_component.proxy != nullptr);

        render_collector.PushEntityToRender(
            entity_id,
            *mesh_component.proxy
        );
    }
    
#ifdef HYP_VISIBILITY_CHECK_DEBUG
    HYP_LOG(Scene, Debug, "Collected {} entities for camera {}, {} skipped", num_collected_entities, camera->GetName(), num_skipped_entities);
#endif

    return render_collector.PushUpdatesToRenderThread(camera, override_attributes);
}

RenderCollector::CollectionResult Scene::CollectDynamicEntities(
    RenderCollector &render_collector,
    const Handle<Camera> &camera,
    const Optional<RenderableAttributeSet> &override_attributes,
    bool skip_frustum_culling
) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!camera.IsValid()) {
        return { };
    }

    const ID<Camera> camera_id = camera.GetID();
    
    const VisibilityStateSnapshot visibility_state_snapshot = m_octree.GetVisibilityState().GetSnapshot(camera_id);

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component, visibility_state_component, _] : m_entity_manager->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::DYNAMIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT)) {
        if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)) {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
            if (!visibility_state_component.visibility_state) {
                continue;
            }


            if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                HYP_LOG(Scene, Debug, "Skipping entity #{} for camera #{} due to visibility state being invalid.",
                    entity_id.Value(), camera_id.Value());
#endif

                continue;
            }
#endif
        }

        AssertThrow(mesh_component.proxy != nullptr);

        render_collector.PushEntityToRender(
            entity_id,
            *mesh_component.proxy
        );
    }

    return render_collector.PushUpdatesToRenderThread(camera, override_attributes);
}

RenderCollector::CollectionResult Scene::CollectStaticEntities(
    RenderCollector &render_collector,
    const Handle<Camera> &camera,
    const Optional<RenderableAttributeSet> &override_attributes,
    bool skip_frustum_culling
) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    if (!camera.IsValid()) {
        // if camera is invalid, update without adding any entities
        return render_collector.PushUpdatesToRenderThread(camera, override_attributes);
    }

    const ID<Camera> camera_id = camera.GetID();
    
    const VisibilityStateSnapshot visibility_state_snapshot = m_octree.GetVisibilityState().GetSnapshot(camera_id);

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component, visibility_state_component, _] : m_entity_manager->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::STATIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT)) {
        if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)) {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
            if (!visibility_state_component.visibility_state) {
                continue;
            }

            if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                HYP_LOG(Scene, Debug, "Skipping entity #{} for camera #{} due to visibility state being invalid.",
                    entity_id.Value(), camera_id.Value());
#endif

                continue;
            }
#endif
        }

        AssertThrow(mesh_component.proxy != nullptr);

        render_collector.PushEntityToRender(
            entity_id,
            *mesh_component.proxy
        );
    }

    return render_collector.PushUpdatesToRenderThread(camera, override_attributes);
}

void Scene::EnqueueRenderUpdates()
{
    HYP_SCOPE;

    AssertReady();

    SceneShaderData shader_data { };
    shader_data.fog_params = Vec4f(float(m_fog_params.color.Packed()), m_fog_params.start_distance, m_fog_params.end_distance, 0.0f);
    shader_data.game_time = m_world != nullptr ? m_world->GetGameState().game_time : 0.0f;

    m_render_resource->SetBufferData(shader_data);
}

void Scene::SetRoot(const NodeProxy &root)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_id);

    if (m_root_node_proxy.IsValid() && m_root_node_proxy->GetScene() == this) {
        m_root_node_proxy->SetScene(nullptr);
    }

    m_root_node_proxy = root;

    if (m_root_node_proxy.IsValid()) {
        m_root_node_proxy->SetScene(this);
    }
}

bool Scene::AddToWorld(World *world)
{
    HYP_SCOPE;
    
    Threads::AssertOnThread(g_game_thread);

    AssertReady();

    if (world == m_world) {
        // World is same, just return true
        return true;
    }

    if (m_world != nullptr) {
        // Can't add to world, world already set
        return false;
    }

    world->AddScene(HandleFromThis());

    return true;
}

bool Scene::RemoveFromWorld()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    AssertReady();

    if (m_world == nullptr) {
        return false;
    }

    m_world->RemoveScene(WeakHandleFromThis());

    return true;
}

template <class SystemType>
void Scene::AddSystemIfApplicable()
{
    UniquePtr<SystemType> system = MakeUnique<SystemType>(*m_entity_manager);

    const EnumFlags<SceneFlags> required_scene_flags = system->GetRequiredSceneFlags();

    if (required_scene_flags != SceneFlags::NONE && !(required_scene_flags & m_flags)) {
        return;
    }

    m_entity_manager->AddSystem(std::move(system));
}

} // namespace hyperion
