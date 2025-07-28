/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/components/AudioComponent.hpp>
#include <scene/components/TransformComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class AudioSystem : public SystemBase
{
    HYP_OBJECT_BODY(AudioSystem);

public:
    AudioSystem(EntityManager& entityManager)
        : SystemBase(entityManager)
    {
    }

    virtual ~AudioSystem() override = default;

    virtual void OnEntityAdded(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<AudioComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ> {}
        };
    }
};

} // namespace hyperion

