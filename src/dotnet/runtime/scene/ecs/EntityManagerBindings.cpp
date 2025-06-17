/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/EntityManager.hpp>

// Components
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT bool EntityManager_HasComponent(EntityManager* manager, uint32 componentTypeId, Entity* entity)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(entity != nullptr);

        return manager->HasComponent(TypeId { componentTypeId }, entity);
    }

    HYP_EXPORT void* EntityManager_GetComponent(EntityManager* manager, uint32 componentTypeId, Entity* entity)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(entity != nullptr);

        return manager->TryGetComponent(TypeId { componentTypeId }, entity).GetPointer();
    }

    HYP_EXPORT void EntityManager_AddComponent(EntityManager* manager, Entity* entity, uint32 componentTypeId, HypData* componentHypData)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(entity != nullptr);
        AssertThrow(componentHypData != nullptr);

        AssertThrow(manager->IsValidComponentType(TypeId { componentTypeId }));

        Handle<Entity> entityHandle = entity->HandleFromThis();

        manager->AddComponent(entityHandle, *componentHypData);
    }

    HYP_EXPORT int8 EntityManager_AddTypedEntity(EntityManager* manager, const HypClass* hypClass, HypData* outHypData)
    {
        AssertThrow(manager != nullptr);
        AssertThrow(hypClass != nullptr);
        AssertThrow(outHypData != nullptr);

        Handle<Entity> entityHandle = manager->AddTypedEntity(hypClass);

        if (!entityHandle.IsValid())
        {
            return false;
        }

        *outHypData = HypData(entityHandle);

        return true;
    }

} // extern "C"
