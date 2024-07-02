/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/world_grid/WorldGrid.hpp>
#include <scene/world_grid/WorldGridPlugin.hpp>

#include <scene/Scene.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/WorldGridComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>

#include <core/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <math/MathUtil.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(WorldGrid, Scene);

#pragma region Helpers

static const FixedArray<WorldGridPatchNeighbor, 8> GetPatchNeighbors(const Vec2i &coord)
{
    return {
        WorldGridPatchNeighbor { coord + Vec2i {  1,  0 } },
        WorldGridPatchNeighbor { coord + Vec2i { -1,  0 } },
        WorldGridPatchNeighbor { coord + Vec2i {  0,  1 } },
        WorldGridPatchNeighbor { coord + Vec2i {  0, -1 } },
        WorldGridPatchNeighbor { coord + Vec2i {  1, -1 } },
        WorldGridPatchNeighbor { coord + Vec2i { -1, -1 } },
        WorldGridPatchNeighbor { coord + Vec2i {  1,  1 } },
        WorldGridPatchNeighbor { coord + Vec2i { -1,  1 } }
    };
}

static Vec2i WorldSpaceToPatchCoord(const WorldGrid &world_grid, const Vec3f &world_position)
{
    Vec3f scaled = world_position - world_grid.GetParams().offset;
    scaled *= Vec3f::One() / (world_grid.GetParams().scale * (Vec3f(world_grid.GetParams().patch_size) - 1.0f));
    scaled = MathUtil::Floor(scaled);

    return Vec2i { int(scaled.x), int(scaled.z) };
}

#pragma endregion Helpers

#pragma region WorldGridPatch

WorldGridPatch::WorldGridPatch(const WorldGridPatchInfo &patch_info)
    : m_patch_info(patch_info)
{
}

WorldGridPatch::~WorldGridPatch()
{
}

#pragma endregion WorldGridPatch

#pragma region WorldGrid

HYP_DEFINE_CLASS(WorldGrid);

WorldGrid::WorldGrid()
    : m_params { },
      m_is_initialized(false)
{
}

WorldGrid::WorldGrid(const WorldGridParams &params, const Handle<Scene> &scene)
    : m_params(params),
      m_scene(scene),
      m_is_initialized(false)
{
}

WorldGrid::~WorldGrid()
{
    Handle<Scene> scene = m_scene.Lock();

    if (scene.IsValid()) {
        const RC<EntityManager> &entity_manager = scene->GetEntityManager();

        if (entity_manager != nullptr) {
            // wait for all commands to finish to prevent holding dangling references
            entity_manager->GetCommandQueue().AwaitEmpty();
        }
    }
}

void WorldGrid::Initialize()
{
    HYP_SCOPE;

    AssertThrow(!m_is_initialized);

    for (const auto &it : m_plugins) {
        it.second->Initialize();
    }

    m_is_initialized = true;
}

void WorldGrid::Shutdown()
{
    HYP_SCOPE;

    AssertThrow(m_is_initialized);

    for (const auto &it : m_plugins) {
        it.second->Shutdown();
    }

    m_plugins.Clear();

    m_is_initialized = false;
}

void WorldGrid::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);

    AssertThrow(m_is_initialized);

    for (const auto &it : m_plugins) {
        HYP_NAMED_SCOPE_FMT("Update Plugin: {}", it.first);

        it.second->Update(delta);
    }
    
    Handle<Scene> scene = m_scene.Lock();
    AssertThrow(scene.IsValid());

    const RC<EntityManager> &entity_manager = scene->GetEntityManager();
    AssertThrow(entity_manager != nullptr);

    if (m_state.patch_generation_queue_shared.has_updates.Exchange(false, MemoryOrder::ACQUIRE_RELEASE)) {
        m_state.patch_generation_queue_shared.mutex.Lock();

        m_state.patch_generation_queue_owned = std::move(m_state.patch_generation_queue_shared.queue);

        m_state.patch_generation_queue_shared.mutex.Unlock();

        while (m_state.patch_generation_queue_owned.Any()) {
            UniquePtr<WorldGridPatch> patch = m_state.patch_generation_queue_owned.Pop();
            const WorldGridPatchInfo &patch_info = patch->GetPatchInfo();

            const auto patch_generation_task_it = m_state.patch_generation_tasks.Find(patch_info.coord);

            if (patch_generation_task_it == m_state.patch_generation_tasks.End()) {
                HYP_LOG(WorldGrid, LogLevel::INFO, "Generation task for patch at {} no longer in map, must have been removed. Skipping.", patch_info.coord);

                continue;
            }

            m_state.patch_generation_tasks.Erase(patch_generation_task_it);

            HYP_LOG(WorldGrid, LogLevel::INFO, "Add completed patch at {}", patch_info.coord);

            // Initialize patch entity on game thread
            entity_manager->PushCommand([&state = m_state, coord = patch_info.coord, patch = std::move(patch), scene](EntityManager &mgr, GameCounter::TickUnit delta) mutable
            {
                const ID<Entity> patch_entity = state.GetPatchEntity(coord);

                if (!patch_entity.IsValid()) {
                    DebugLog(
                        LogType::Warn,
                        "Patch entity at [%d, %d] was not found when updating mesh\n",
                        coord.x, coord.y
                    );

                    return;
                }

                // Prevent patch entity from being removed while updating
                Mutex::Guard guard(state.patch_entities_mutex);

                patch->InitializeEntity(scene, patch_entity);
            });
        }
    }

    const Handle<Camera> &camera = scene->GetCamera();
    const Vec3f camera_position = camera.IsValid() ? camera->GetTranslation() : Vec3f::Zero();
    const Vec2i camera_patch_coord = WorldSpaceToPatchCoord(*this, camera_position);

    if (!m_state.GetPatchEntity(camera_patch_coord).IsValid()) {
        // Enqueue a patch to be created at the current camera position
        if (!m_state.queued_neighbors.Contains(camera_patch_coord)) {
            m_state.patch_update_queue.Push({
                .coord  = camera_patch_coord,
                .state  = WorldGridPatchState::WAITING
            });

            m_state.queued_neighbors.Insert(camera_patch_coord);
        }
    }

    Array<Vec2i> patch_coords_in_range;

    for (int x = MathUtil::Floor(-m_params.max_distance); x <= MathUtil::Ceil(m_params.max_distance) + 1; ++x) {
        for (int z = MathUtil::Floor(-m_params.max_distance); z <= MathUtil::Ceil(m_params.max_distance) + 1; ++z) {
            patch_coords_in_range.PushBack(camera_patch_coord + Vec2i { x, z });
        }
    }

    Array<Vec2i> patch_coords_to_add = patch_coords_in_range;

    // state machine for patch updates
    while (m_state.patch_update_queue.Any()) {
        const WorldGridPatchUpdate update = m_state.patch_update_queue.Pop();

        switch (update.state) {
        case WorldGridPatchState::WAITING:
        {
            HYP_LOG(WorldGrid, LogLevel::INFO, "Add patch at {}", update.coord);

            const WorldGridPatchInfo patch_info {
                .extent     = m_params.patch_size,
                .coord      = update.coord,
                .scale      = m_params.scale,
                .state      = WorldGridPatchState::LOADED,
                .neighbors  = GetPatchNeighbors(update.coord)
            };

            // add command to add the entity
            entity_manager->PushCommand([&state = m_state, &params = m_params, patch_info](EntityManager &mgr, GameCounter::TickUnit delta)
            {
                const ID<Entity> patch_entity = mgr.AddEntity();

                // Add WorldGridPatchComponent
                mgr.AddComponent(patch_entity, WorldGridPatchComponent {
                    .patch_info = patch_info
                });

                // Add TransformComponent
                mgr.AddComponent(patch_entity, TransformComponent {
                    .transform = Transform {
                        Vec3f {
                            params.offset.x + (float(patch_info.coord.x) - 0.5f) * (Vec3f(patch_info.extent).Max() - 1.0f) * patch_info.scale.x,
                            params.offset.y,
                            params.offset.z + (float(patch_info.coord.y) - 0.5f) * (Vec3f(patch_info.extent).Max() - 1.0f) * patch_info.scale.z
                        }
                    }
                });

                // Add VisibilityStateComponent
                mgr.AddComponent(patch_entity, VisibilityStateComponent { });

                // Add BoundingBoxComponent
                mgr.AddComponent(patch_entity, BoundingBoxComponent { });

                HYP_LOG(WorldGrid, LogLevel::INFO, "Patch entity at {} added", patch_info.coord);

                state.AddPatchEntity(patch_entity, patch_info.coord);
            });

            if (RC<WorldGridPlugin> plugin = GetMainPlugin()) {
                // add task to generation queue
                Task<void> generation_task = TaskSystem::GetInstance().Enqueue([patch_info, &generation_queue = m_state.patch_generation_queue_shared, plugin = std::move(plugin)]()
                {
                    HYP_NAMED_SCOPE_FMT("Generating patch at {}", patch_info.coord);

                    Mutex::Guard guard(generation_queue.mutex);
                    generation_queue.queue.Push(plugin->CreatePatch(patch_info));
                    generation_queue.has_updates.Set(true, MemoryOrder::RELEASE);

                    HYP_LOG(WorldGrid, LogLevel::INFO, "Patch generation at {} completed", patch_info.coord);
                });

                auto it = m_state.patch_generation_tasks.Find(patch_info.coord);

                if (it != m_state.patch_generation_tasks.End()) {
                    if (!it->second.Cancel()) {
                        HYP_LOG(WorldGrid, LogLevel::WARNING, "Failed to cancel patch generation task at {}", patch_info.coord);
                    }

                    it->second = std::move(generation_task);
                } else {
                    m_state.patch_generation_tasks.Insert(patch_info.coord, std::move(generation_task));
                }
            } else {
                HYP_LOG(WorldGrid, LogLevel::WARNING, "No main plugin found to generate patch at {}", patch_info.coord);
            }

            break;
        }
        case WorldGridPatchState::UNLOADED:
        {
            HYP_LOG(WorldGrid, LogLevel::INFO, "Unload patch at {}", update.coord);

            const auto patch_generation_task_it = m_state.patch_generation_tasks.Find(update.coord);

            if (patch_generation_task_it != m_state.patch_generation_tasks.End()) {
                Task<void> &patch_generation_task = patch_generation_task_it->second;

                if (!patch_generation_task.Cancel()) {
                    HYP_LOG(WorldGrid, LogLevel::WARNING, "Failed to cancel patch generation task at {}", update.coord);
                }

                m_state.patch_generation_tasks.Erase(patch_generation_task_it);
            }

            const auto queued_neighbors_it = m_state.queued_neighbors.Find(update.coord);

            if (queued_neighbors_it != m_state.queued_neighbors.End()) {
                m_state.queued_neighbors.Erase(queued_neighbors_it);
            }

            // Push command to remove the entity
            entity_manager->PushCommand([&state = m_state, update](EntityManager &mgr, GameCounter::TickUnit delta)
            {
                const ID<Entity> patch_entity = state.GetPatchEntity(update.coord);

                if (!patch_entity.IsValid()) {
                    HYP_LOG(WorldGrid, LogLevel::INFO, "Patch entity at {} was not found when unloading", update.coord);

                    return;
                }

                AssertThrowMsg(
                    state.RemovePatchEntity(patch_entity),
                    "Failed to remove patch entity at [%d, %d]",
                    update.coord.x,
                    update.coord.y
                );

                if (mgr.HasEntity(patch_entity)) {
                    mgr.RemoveEntity(patch_entity);
                }

                HYP_LOG(WorldGrid, LogLevel::INFO, "Patch entity at {} removed", update.coord);
            });

            break;
        }
        default:
        {
            // Push command to update patch state
            entity_manager->PushCommand([&state = m_state, update](EntityManager &mgr, GameCounter::TickUnit delta)
            {
                const ID<Entity> patch_entity = state.GetPatchEntity(update.coord);

                if (!patch_entity.IsValid()) {
                    HYP_LOG(WorldGrid, LogLevel::WARNING, "Patch entity at {} was not found when updating state", update.coord);

                    return;
                }

                WorldGridPatchComponent *patch_component = mgr.TryGetComponent<WorldGridPatchComponent>(patch_entity);
                AssertThrowMsg(patch_component, "Patch entity did not have a WorldGridPatchComponent when updating state");

                // set new state
                patch_component->patch_info.state = update.state;
            });

            break;
        }
        }

        {
            Mutex::Guard guard(m_state.patch_entities_mutex);

            for (auto &it : m_state.patch_entities) {
                const auto in_range_it = patch_coords_in_range.Find(it.first);

                const bool is_in_range = in_range_it != patch_coords_in_range.End();

                if (is_in_range) {
                    patch_coords_to_add.Erase(it.first);
                }

                // Push command to update patch state
                // @FIXME: thread safety issues with using state here
                entity_manager->PushCommand([patch_coord = it.first, is_in_range, &state = m_state](EntityManager &mgr, GameCounter::TickUnit delta)
                {
                    const ID<Entity> entity = state.GetPatchEntity(patch_coord);

                    if (!entity.IsValid()) {
                        // Patch entity was removed, skip
                        return;
                    }

                    WorldGridPatchComponent *world_grid_patch_component = mgr.TryGetComponent<WorldGridPatchComponent>(entity);

                    AssertThrowMsg(
                        world_grid_patch_component,
                        "Patch entity at [%d, %d] did not have a WorldGridPatchComponent when updating state",
                        patch_coord.x,
                        patch_coord.y
                    );

                    const WorldGridPatchInfo &patch_info = world_grid_patch_component->patch_info;

                    switch (patch_info.state) {
                    case WorldGridPatchState::LOADED:
                    {
                        // reset unload timer
                        world_grid_patch_component->patch_info.unload_timer = 0.0f;

                        auto queued_neighbors_it = state.queued_neighbors.Find(patch_info.coord);

                        // Remove loaded patch from queued neighbors
                        if (queued_neighbors_it != state.queued_neighbors.End()) {
                            state.queued_neighbors.Erase(queued_neighbors_it);
                        }

                        if (!is_in_range) {
                            HYP_LOG(WorldGrid, LogLevel::INFO, "Patch {} no longer in range, unloading", patch_info.coord);

                            // Start unloading
                            state.patch_update_queue.Push({
                                .coord  = patch_info.coord,
                                .state  = WorldGridPatchState::UNLOADING
                            });
                        }

                        break;
                    }
                    case WorldGridPatchState::UNLOADING:
                        if (is_in_range) {
                            HYP_LOG(WorldGrid, LogLevel::INFO, "Patch {} back in range, stopping unloading", patch_info.coord);

                            // Stop unloading
                            state.patch_update_queue.Push({
                                .coord  = patch_info.coord,
                                .state  = WorldGridPatchState::LOADED
                            });
                        } else {
                            world_grid_patch_component->patch_info.unload_timer += delta;

                            if (world_grid_patch_component->patch_info.unload_timer >= 10.0f) {
                                HYP_LOG(WorldGrid, LogLevel::INFO, "Unloading patch at {}", patch_info.coord);

                                // Unload it now
                                state.patch_update_queue.Push({
                                    .coord  = patch_info.coord,
                                    .state  = WorldGridPatchState::UNLOADED
                                });
                            }
                        }

                        break;
                    default:
                        break;
                    }
                });
            }
        }

        // enqueue a patch to be created for each patch in range
        for (const Vec2i &coord : patch_coords_to_add) {
            if (!m_state.queued_neighbors.Contains(coord)) {
                m_state.patch_update_queue.Push({
                    .coord  = coord,
                    .state  = WorldGridPatchState::WAITING
                });

                m_state.queued_neighbors.Insert(coord);
            }
        }
    }
}

void WorldGrid::AddPlugin(int priority, RC<WorldGridPlugin> &&plugin)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertThrow(plugin != nullptr);

    if (m_is_initialized) {
        // Initialize plugin if grid is already initialized

        plugin->Initialize();
    }

    m_plugins.Set(priority, std::move(plugin));
}

RC<WorldGridPlugin> WorldGrid::GetPlugin(int priority) const
{
    const auto it = m_plugins.Find(priority);

    if (it == m_plugins.End()) {
        return nullptr;
    }

    return it->second;
}

RC<WorldGridPlugin> WorldGrid::GetMainPlugin() const
{
    if (m_plugins.Empty()) {
        return nullptr;
    }

    return m_plugins.Front().second;
}

#pragma endregion WorldGrid

} // namespace hyperion
