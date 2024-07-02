/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/TerrainSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <core/utilities/Format.hpp>

#include <Engine.hpp>

namespace hyperion {

void TerrainSystem::Process(GameCounter::TickUnit delta)
{
#if 0
    for (auto [entity_id, terrain_component, transform_component, mesh_component] : GetEntityManager().GetEntitySet<TerrainComponent, TransformComponent, MeshComponent>()) {
        if (!(terrain_component.flags & TERRAIN_COMPONENT_FLAG_INIT)) {
            mesh_component.material = CreateObject<Material>(NAME("terrain_material"));
            mesh_component.material->SetBucket(BUCKET_OPAQUE);
            mesh_component.material->SetIsDepthTestEnabled(true);
            mesh_component.material->SetIsDepthWriteEnabled(true);
            mesh_component.material->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.85f);
            mesh_component.material->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
            mesh_component.material->SetParameter(Material::MATERIAL_KEY_UV_SCALE, 1.0f);

            if (auto albedo_texture_asset = g_asset_manager->Load<Texture>("textures/mossy-ground1-Unity/mossy-ground1-albedo.png")) {
                Handle<Texture> albedo_texture = albedo_texture_asset.Result();
                albedo_texture->GetImage()->SetIsSRGB(true);
                mesh_component.material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, std::move(albedo_texture));
            }

            if (auto ground_texture_asset = g_asset_manager->Load<Texture>("textures/mossy-ground1-Unity/mossy-ground1-preview.png")) {
                mesh_component.material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, ground_texture_asset.Result());
            }

            InitObject(mesh_component.material);

            mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;

            terrain_component.flags |= TERRAIN_COMPONENT_FLAG_INIT;

            DebugLog(
                LogType::Info,
                "Terrain entity [%u] initialized\n",
                entity_id.Value()
            );
        }

        RC<TerrainGenerationState> &state = m_states[entity_id];

        if (!state) {
            state.Reset(new TerrainGenerationState());

            state->patch_generation_queue_shared.Reset(new TerrainGenerationQueue());

            state->noise_combinator.Reset(new NoiseCombinator(terrain_component.seed));
            (*state->noise_combinator)
                .Use<WorleyNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, mountain_height, 0.0f, Vector3(0.35f, 0.35f, 0.0f) * global_terrain_noise_scale)
                // .Use<SimplexNoiseGenerator>(1, NoiseCombinator::Mode::MULTIPLICATIVE, 0.5f, 0.5f, Vector3(50.0f, 50.0f, 0.0f) * global_terrain_noise_scale)
                .Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, base_height, 0.0f, Vector3(100.0f, 100.0f, 0.0f) * global_terrain_noise_scale)
                .Use<SimplexNoiseGenerator>(3, NoiseCombinator::Mode::ADDITIVE, base_height * 0.5f, 0.0f, Vector3(50.0f, 50.0f, 0.0f) * global_terrain_noise_scale)
                .Use<SimplexNoiseGenerator>(4, NoiseCombinator::Mode::ADDITIVE, base_height * 0.25f, 0.0f, Vector3(25.0f, 25.0f, 0.0f) * global_terrain_noise_scale)
                .Use<SimplexNoiseGenerator>(5, NoiseCombinator::Mode::ADDITIVE, base_height * 0.125f, 0.0f, Vector3(12.5f, 12.5f, 0.0f) * global_terrain_noise_scale)
                .Use<SimplexNoiseGenerator>(6, NoiseCombinator::Mode::ADDITIVE, base_height * 0.06f, 0.0f, Vector3(6.25f, 6.25f, 0.0f) * global_terrain_noise_scale)
                .Use<SimplexNoiseGenerator>(7, NoiseCombinator::Mode::ADDITIVE, base_height * 0.03f, 0.0f, Vector3(3.125f, 3.125f, 0.0f) * global_terrain_noise_scale)
                .Use<SimplexNoiseGenerator>(8, NoiseCombinator::Mode::ADDITIVE, base_height * 0.015f, 0.0f, Vector3(1.56f, 1.56f, 0.0f) * global_terrain_noise_scale);
        }

        if (state->patch_generation_queue_shared->has_updates.Exchange(false, MemoryOrder::ACQUIRE_RELEASE)) {
            state->patch_generation_queue_shared->mutex.Lock();

            state->patch_generation_queue_owned = std::move(state->patch_generation_queue_shared->queue);

            state->patch_generation_queue_shared->mutex.Unlock();

            while (state->patch_generation_queue_owned.Any()) {
                TerrainGenerationResult result = state->patch_generation_queue_owned.Pop();
                const TerrainPatchInfo &patch_info = result.patch_info;

                const auto patch_generation_task_it = state->patch_generation_tasks.Find(patch_info.coord);

                if (patch_generation_task_it == state->patch_generation_tasks.End()) {
                    DebugLog(
                        LogType::Info,
                        "Generation task for patch coord [%d, %d] no longer in map, must have been removed. Skipping.\n",
                        patch_info.coord.x,
                        patch_info.coord.y
                    );

                    continue;
                }

                state->patch_generation_tasks.Erase(patch_generation_task_it);

                DebugLog(
                    LogType::Debug,
                    "Add completed terrain mesh at coord [%d, %d]\n",
                    patch_info.coord.x,
                    patch_info.coord.y
                );

                Handle<Mesh> mesh = std::move(result.mesh);
                AssertThrowMsg(mesh.IsValid(), "Terrain mesh is invalid");
                InitObject(mesh);

                GetEntityManager().PushCommand([state, coord = patch_info.coord, mesh = std::move(mesh), material = mesh_component.material](EntityManager &mgr, GameCounter::TickUnit delta) mutable
                {
                    const ID<Entity> patch_entity = state->GetPatchEntity(coord);

                    if (!patch_entity.IsValid()) {
                        DebugLog(
                            LogType::Warn,
                            "Patch entity at [%d, %d] was not found when updating mesh\n",
                            coord.x, coord.y
                        );

                        return;
                    }

                    // Prevent patch entity from being removed while updating
                    Mutex::Guard guard(state->patch_entities_mutex);

                    MeshComponent *patch_mesh_component = mgr.TryGetComponent<MeshComponent>(patch_entity);
                    BoundingBoxComponent *patch_bounding_box_component = mgr.TryGetComponent<BoundingBoxComponent>(patch_entity);
                    TransformComponent *patch_transform_component = mgr.TryGetComponent<TransformComponent>(patch_entity);

                    if (patch_bounding_box_component) {
                        patch_bounding_box_component->local_aabb = mesh->GetAABB();

                        if (patch_transform_component) {
                            patch_bounding_box_component->world_aabb = BoundingBox::Empty();

                            for (const Vec3f &corner : patch_bounding_box_component->local_aabb.GetCorners()) {
                                patch_bounding_box_component->world_aabb.Extend(patch_transform_component->transform.GetMatrix() * corner);
                            }
                        }
                    }

                    if (patch_mesh_component) {
                        patch_mesh_component->mesh = std::move(mesh);
                        patch_mesh_component->material = std::move(material);
                        patch_mesh_component->flags |= MESH_COMPONENT_FLAG_DIRTY;
                    } else {
                        // Add MeshComponent to patch entity
                        mgr.AddComponent<MeshComponent>(patch_entity, MeshComponent {
                            .mesh       = std::move(mesh),
                            .material   = std::move(material)
                        });
                    }
                });
            }
        }

        const Handle<Camera> &camera = GetEntityManager().GetScene()->GetCamera();
        const Vec3f camera_position = camera.IsValid() ? camera->GetTranslation() : Vec3f::Zero();

        const TerrainPatchCoord camera_patch_coord = WorldSpaceToPatchCoord(camera_position, terrain_component, transform_component);

        if (!state->GetPatchEntity(camera_patch_coord).IsValid()) {
            // Enqueue a patch to be created at the current camera position
            if (!state->queued_neighbors.Contains(camera_patch_coord)) {
                state->patch_update_queue.Push({
                    .coord  = camera_patch_coord,
                    .state  = TerrainPatchState::WAITING
                });

                state->queued_neighbors.Insert(camera_patch_coord);
            }
        }

        Array<TerrainPatchCoord> patch_coords_in_range;

        for (int x = MathUtil::Floor(-terrain_component.max_distance); x <= MathUtil::Ceil(terrain_component.max_distance) + 1; ++x) {
            for (int z = MathUtil::Floor(-terrain_component.max_distance); z <= MathUtil::Ceil(terrain_component.max_distance) + 1; ++z) {
                patch_coords_in_range.PushBack(camera_patch_coord + TerrainPatchCoord { x, z });
            }
        }

        Array<TerrainPatchCoord> patch_coords_to_add(patch_coords_in_range);

        // Handle patch states
        while (state->patch_update_queue.Any()) {
            const TerrainPatchUpdate update = state->patch_update_queue.Pop();

            switch (update.state) {
            case TerrainPatchState::WAITING: {
                //AddPatch(update.coord);

                DebugLog(LogType::Debug, "Add patch at [%d, %d]\n", update.coord.x, update.coord.y);

                const TerrainPatchInfo patch_info {
                    .extent     = terrain_component.patch_size,
                    .coord      = update.coord,
                    .scale      = terrain_component.scale,
                    .state      = TerrainPatchState::LOADED,
                    .neighbors  = GetPatchNeighbors(update.coord)
                };

                // add command to add the entity
                GetEntityManager().PushCommand([state, patch_info, translation = transform_component.transform.GetTranslation()](EntityManager &mgr, GameCounter::TickUnit delta)
                {
                    const ID<Entity> patch_entity = mgr.AddEntity();

                    // Add TerrainPatchComponent
                    mgr.AddComponent<TerrainPatchComponent>(patch_entity, TerrainPatchComponent {
                        .patch_info = patch_info
                    });

                    // Add TransformComponent
                    mgr.AddComponent<TransformComponent>(patch_entity, TransformComponent {
                        .transform = Transform {
                            Vec3f {
                                translation.x + (float(patch_info.coord.x) - 0.5f) * (Vec3f(patch_info.extent).Max() - 1.0f) * patch_info.scale.x,
                                translation.y,
                                translation.z + (float(patch_info.coord.y) - 0.5f) * (Vec3f(patch_info.extent).Max() - 1.0f) * patch_info.scale.z
                            }
                        }
                    });

                    // Add VisibilityStateComponent
                    mgr.AddComponent<VisibilityStateComponent>(patch_entity, VisibilityStateComponent { });

                    // Add BoundingBoxComponent
                    mgr.AddComponent<BoundingBoxComponent>(patch_entity, BoundingBoxComponent { });

                    DebugLog(LogType::Debug, "Patch entity at [%d, %d] added\n", patch_info.coord.x, patch_info.coord.y);

                    state->AddPatchEntity(patch_entity, patch_info.coord);
                });

                // add task to generation queue
                Task<void> generation_task = TaskSystem::GetInstance().Enqueue([patch_info, generation_queue = state->patch_generation_queue_shared, noise_combinator = state->noise_combinator]()
                {
                    HYP_NAMED_SCOPE_FMT("Generating Terrain Mesh {}", patch_info.coord);

                    terrain::TerrainMeshBuilder mesh_builder(patch_info);
                    mesh_builder.GenerateHeights(*noise_combinator);

                    Handle<Mesh> mesh = mesh_builder.BuildMesh();
                    AssertThrowMsg(mesh.IsValid(), "Generated terrain mesh is invalid");

                    DebugLog(LogType::Debug, "From thread: %s\tTerrain mesh has %u indices\n", Threads::CurrentThreadID().name.LookupString(), mesh->NumIndices());

                    Mutex::Guard guard(generation_queue->mutex);

                    generation_queue->queue.Push(TerrainGenerationResult {
                        .patch_info = patch_info,
                        .mesh       = std::move(mesh)
                    });

                    generation_queue->has_updates.Set(true, MemoryOrder::RELEASE);

                    DebugLog(
                        LogType::Debug,
                        "Terrain mesh at coord [%d, %d] generation completed\n",
                        patch_info.coord.x,
                        patch_info.coord.y
                    );
                });

                state->patch_generation_tasks.Insert(patch_info.coord, std::move(generation_task));

                break;
            }
            case TerrainPatchState::UNLOADED: {
                DebugLog(
                    LogType::Debug,
                    "Unload patch at [%d, %d]\n",
                    update.coord.x, update.coord.y
                );

                const auto patch_generation_task_it = state->patch_generation_tasks.Find(update.coord);

                if (patch_generation_task_it != state->patch_generation_tasks.End()) {
                    const Task<void> &patch_generation_task = patch_generation_task_it->second;

                    TaskSystem::GetInstance().CancelTask(patch_generation_task);

                    state->patch_generation_tasks.Erase(patch_generation_task_it);
                }

                const auto queued_neighbors_it = state->queued_neighbors.Find(update.coord);

                if (queued_neighbors_it != state->queued_neighbors.End()) {
                    state->queued_neighbors.Erase(queued_neighbors_it);
                }

                // Push command to remove the entity
                GetEntityManager().PushCommand([state, update](EntityManager &mgr, GameCounter::TickUnit delta)
                {
                    const ID<Entity> patch_entity = state->GetPatchEntity(update.coord);

                    if (!patch_entity.IsValid()) {
                        DebugLog(
                            LogType::Warn,
                            "Patch entity at [%d, %d] was not found when unloading\n",
                            update.coord.x, update.coord.y
                        );

                        return;
                    }

                    AssertThrowMsg(
                        state->RemovePatchEntity(patch_entity),
                        "Failed to remove patch entity at [%d, %d]",
                        update.coord.x,
                        update.coord.y
                    );

                    if (mgr.HasEntity(patch_entity)) {
                        mgr.RemoveEntity(patch_entity);
                    }

                    DebugLog(
                        LogType::Debug,
                        "Patch entity at [%d, %d] removed\n",
                        update.coord.x, update.coord.y
                    );
                });

                break;
            }
            default: {
                // Push command to update patch state
                GetEntityManager().PushCommand([state, update](EntityManager &mgr, GameCounter::TickUnit delta)
                {
                    const ID<Entity> patch_entity = state->GetPatchEntity(update.coord);

                    if (!patch_entity.IsValid()) {
                        DebugLog(
                            LogType::Warn,
                            "Patch entity at [%d, %d] was not found when updating state\n",
                            update.coord.x, update.coord.y
                        );

                        return;
                    }

                    TerrainPatchComponent *patch_component = mgr.TryGetComponent<TerrainPatchComponent>(patch_entity);
                    AssertThrowMsg(patch_component, "Patch entity did not have a TerrainPatchComponent when updating state");

                    // set new state
                    patch_component->patch_info.state = update.state;
                });

                break;
            }
            }
        }

        {
            Mutex::Guard guard(state->patch_entities_mutex);

            for (auto &it : state->patch_entities) {
                const auto in_range_it = patch_coords_in_range.Find(it.first);

                const bool is_in_range = in_range_it != patch_coords_in_range.End();

                if (is_in_range) {
                    patch_coords_to_add.Erase(it.first);
                }

                // Push command to update patch state
                // @FIXME: thread safety issues with using state here
                GetEntityManager().PushCommand([patch_coord = it.first, is_in_range, state](EntityManager &mgr, GameCounter::TickUnit delta)
                {
                    const ID<Entity> entity = state->GetPatchEntity(patch_coord);

                    if (!entity.IsValid()) {
                        // Patch entity was removed, skip
                        return;
                    }

                    TerrainPatchComponent *terrian_patch_component = mgr.TryGetComponent<TerrainPatchComponent>(entity);

                    AssertThrowMsg(
                        terrian_patch_component,
                        "Patch entity at [%d, %d] did not have a TerrainPatchComponent when updating state",
                        patch_coord.x,
                        patch_coord.y
                    );

                    const TerrainPatchInfo &patch_info = terrian_patch_component->patch_info;

                    switch (patch_info.state) {
                    case TerrainPatchState::LOADED: {
                        // reset unload timer
                        terrian_patch_component->patch_info.unload_timer = 0.0f;

                        auto queued_neighbors_it = state->queued_neighbors.Find(patch_info.coord);

                        // Remove loaded patch from queued neighbors
                        if (queued_neighbors_it != state->queued_neighbors.End()) {
                            state->queued_neighbors.Erase(queued_neighbors_it);
                        }

                        if (!is_in_range) {
                            DebugLog(
                                LogType::Debug,
                                "Patch [%d, %d] no longer in range, unloading\n",
                                patch_info.coord.x, patch_info.coord.y
                            );

                            // Start unloading
                            state->patch_update_queue.Push({
                                .coord  = patch_info.coord,
                                .state  = TerrainPatchState::UNLOADING
                            });
                        }

                        break;
                    }
                    case TerrainPatchState::UNLOADING:
                        if (is_in_range) {
                            DebugLog(
                                LogType::Debug,
                                "Patch [%d, %d] back in range, stopping unloading\n",
                                patch_info.coord.x, patch_info.coord.y
                            );

                            // Stop unloading
                            state->patch_update_queue.Push({
                                .coord  = patch_info.coord,
                                .state  = TerrainPatchState::LOADED
                            });
                        } else {
                            terrian_patch_component->patch_info.unload_timer += delta;

                            if (terrian_patch_component->patch_info.unload_timer >= 10.0f) {
                                DebugLog(
                                    LogType::Debug,
                                    "Unloading patch at [%d, %d]\n",
                                    patch_info.coord.x, patch_info.coord.y
                                );

                                // Unload it now
                                state->patch_update_queue.Push({
                                    .coord  = patch_info.coord,
                                    .state  = TerrainPatchState::UNLOADED
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
        for (const TerrainPatchCoord &coord : patch_coords_to_add) {
            if (!state->queued_neighbors.Contains(coord)) {
                state->patch_update_queue.Push({
                    .coord  = coord,
                    .state  = TerrainPatchState::WAITING
                });

                state->queued_neighbors.Insert(coord);
            }
        }
    }
#endif
}

} // namespace hyperion
