/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/EntityTag.hpp>
#include <scene/components/SkyComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/MeshComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class SkySystem : public SystemBase
{
    HYP_OBJECT_BODY(SkySystem);

public:
    SkySystem(EntityManager& entityManager);
    virtual ~SkySystem() override = default;

    virtual bool RequiresGameThread() const override
    {
        return true;
    }

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<SkyComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},

            // calling EnvProbe::Update() calls View::Update() which reads the following components on entities it processes.
            ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ, false> {},

            ComponentDescriptor<EntityTagComponent<EntityTag::STATIC>, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }

    void AddRenderSubsystemToEnvironment(World* world, EntityManager& mgr, Entity* entity, SkyComponent& skyComponent, MeshComponent* meshComponent);
};

} // namespace hyperion

