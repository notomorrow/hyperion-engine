/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_TERRAIN_SYSTEM_HPP
#define HYPERION_ECS_TERRAIN_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/TerrainComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>

#include <core/threading/TaskSystem.hpp>

#include <util/NoiseFactory.hpp>

namespace hyperion {

class TerrainSystem : public System<
    ComponentDescriptor<TerrainComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE>
>
{
public:
    virtual ~TerrainSystem() override = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;

private:
    struct TerrainGenerationResult
    {
        TerrainPatchInfo    patch_info;
        Handle<Mesh>        mesh;
    };

    struct TerrainPatchUpdate
    {
        TerrainPatchCoord   coord;
        TerrainPatchState   state;
    };

    struct TerrainGenerationQueue
    {
        Mutex                           mutex;
        Queue<TerrainGenerationResult>  queue;
        AtomicVar<bool>                 has_updates;
    };

    struct TerrainGenerationState
    {
        FlatMap<TerrainPatchCoord, TaskRef>     patch_generation_tasks;
        Queue<TerrainGenerationResult>          patch_generation_queue_owned;
        RC<TerrainGenerationQueue>              patch_generation_queue_shared;

        FlatSet<TerrainPatchCoord>              queued_neighbors;

        Queue<TerrainPatchUpdate>               patch_update_queue;

        RC<NoiseCombinator>                     noise_combinator;

        FlatMap<TerrainPatchCoord, ID<Entity>>  patch_entities;
        mutable Mutex                           patch_entities_mutex;

        void AddPatchEntity(ID<Entity> entity, const TerrainPatchCoord &coord)
        {
            Mutex::Guard guard(patch_entities_mutex);

            patch_entities.Insert(coord, entity);
        }

        bool RemovePatchEntity(ID<Entity> entity)
        {
            Mutex::Guard guard(patch_entities_mutex);

            for (auto it = patch_entities.Begin(); it != patch_entities.End(); ++it) {
                if (it->second == entity) {
                    patch_entities.Erase(it);

                    return true;
                }
            }

            return false;
        }

        bool RemovePatchEntity(const TerrainPatchCoord &coord)
        {
            Mutex::Guard guard(patch_entities_mutex);

            return patch_entities.Erase(coord);
        }

        ID<Entity> GetPatchEntity(const TerrainPatchCoord &coord) const
        {
            Mutex::Guard guard(patch_entities_mutex);

            const auto it = patch_entities.Find(coord);

            if (it != patch_entities.End()) {
                return it->second;
            }

            return {};
        }
    };

    HashMap<ID<Entity>, RC<TerrainGenerationState>> m_states;
};

} // namespace hyperion

#endif