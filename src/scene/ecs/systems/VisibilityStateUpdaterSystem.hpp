/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_VISIBILITY_STATE_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_VISIBILITY_STATE_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

namespace hyperion {

class VisibilityStateUpdaterSystem : public System<
    ComponentDescriptor<VisibilityStateComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    virtual ~VisibilityStateUpdaterSystem() override = default;

    virtual void OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity) override;
    virtual void OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity) override;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif