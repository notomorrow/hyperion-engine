#ifndef HYPERION_V2_ECS_LIGHT_VISIBILITY_UPDATER_SYSTEM_HPP
#define HYPERION_V2_ECS_LIGHT_VISIBILITY_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <rendering/EntityDrawData.hpp>

namespace hyperion::v2 {

class LightVisibilityUpdaterSystem : public System<
    ComponentDescriptor<LightComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,

    // Can read and write the VisibilityStateComponent but does not receive events
    ComponentDescriptor<VisibilityStateComponent, COMPONENT_RW_FLAGS_READ_WRITE, false>,
    // Can read the MeshComponent but does not receive events (uses material)
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ, false>
>
{
public:
    virtual ~LightVisibilityUpdaterSystem() override = default;

    virtual void OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity) override;
    virtual void OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity) override;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif