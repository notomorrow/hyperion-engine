/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENV_GRID_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_ENV_GRID_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/EnvGridComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/EntityTag.hpp>

namespace hyperion {

class EnvGridUpdaterSystem : public System<
    EnvGridUpdaterSystem,
    ComponentDescriptor<EnvGridComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,
    ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ>,

    ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_ENV_GRID_TRANSFORM>, COMPONENT_RW_FLAGS_READ, false>,
    ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_ENV_GRID>, COMPONENT_RW_FLAGS_READ, false>
>
{
public:
    EnvGridUpdaterSystem(EntityManager &entity_manager);
    virtual ~EnvGridUpdaterSystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;

private:
    void UpdateEnvGrid(GameCounter::TickUnit delta, EnvGridComponent &env_grid_component, BoundingBoxComponent &bounding_box_component);

    void AddRenderSubsystemToEnvironment(EnvGridComponent &env_grid_component, BoundingBoxComponent &bounding_box_component, World *world);
};

} // namespace hyperion

#endif