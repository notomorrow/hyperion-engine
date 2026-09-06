/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/LayerOverrideSystem.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Field.hpp>

#include <LayerOverrideSystem.generated.inl>

namespace Hyperion {

namespace Helpers
{
namespace
{

inline EntityLayerOverrideSet* FindLayerOverrideSet(Array<EntityLayerOverrideSet>& sets, Name layerName)
{
    for (EntityLayerOverrideSet& set : sets)
    {
        if (set.layerName == layerName)
        {
            return &set;
        }
    }

    return nullptr;
}

inline const EntityLayerOverrideSet* FindLayerOverrideSet(const Array<EntityLayerOverrideSet>& sets, Name layerName)
{
    for (const EntityLayerOverrideSet& set : sets)
    {
        if (set.layerName == layerName)
        {
            return &set;
        }
    }

    return nullptr;
}

inline bool HasLayerOverrideSet(const Array<EntityLayerOverrideSet>& sets, Name layerName)
{
    return FindLayerOverrideSet(sets, layerName) != nullptr;
}

inline bool IsPropertyOverriddenInLayer(const Array<EntityLayerOverrideSet>& sets, Name layerName, Name propertyName)
{
    const EntityLayerOverrideSet* set = FindLayerOverrideSet(sets, layerName);

    if (!set)
    {
        return false;
    }

    for (const LayerPropertyOverride& overrideEntry : set->propertyOverrides)
    {
        if (overrideEntry.property == propertyName)
        {
            return true;
        }
    }

    return false;
}

inline bool GetLayerOverrideValue(const Array<EntityLayerOverrideSet>& sets, Name layerName, Name propertyName, BoxedValue& outValue)
{
    const EntityLayerOverrideSet* set = FindLayerOverrideSet(sets, layerName);

    if (!set)
    {
        return false;
    }

    for (const LayerPropertyOverride& overrideEntry : set->propertyOverrides)
    {
        if (overrideEntry.property == propertyName)
        {
            outValue = overrideEntry.value;

            return true;
        }
    }

    return false;
}

//-- Reflection

const IMember* ResolveOverridableMember(const Class* cls, Name propertyName)
{
    if (!cls || !propertyName)
    {
        return nullptr;
    }

    // Entity name is not overridable
    if (propertyName == NAME("Name"))
    {
        return nullptr;
    }

    const IMember* member = cls->GetMember(StringHash(propertyName), MemberType::Field | MemberType::Property);

    if (!member)
    {
        return nullptr;
    }

    // If has `Property = ...`, use the synthetic Property instead
    if (member->GetMemberType() == MemberType::Field && member->GetAttribute(Attributes::g_attrProperty).IsValid())
    {
        if (Property* prop = cls->GetProperty(StringHash(propertyName)))
        {
            member = prop;
        }
    }

    if (member->GetAttribute(Attributes::g_attrJsonIgnore).IsValid())
    {
        return nullptr;
    }

    if (member->GetMemberType() == MemberType::Property && !static_cast<const Property*>(member)->CanSet())
    {
        return nullptr;
    }

    return member;
}

BoxedValue GetEntityMemberValue(const IMember* member, Entity* entity)
{
    BoxedValue target = BoxedValue(entity->HandleFromThis());

    switch (member->GetMemberType())
    {
    case MemberType::Property:
        return static_cast<const Property*>(member)->Get(target);
    case MemberType::Field:
        return static_cast<const Field*>(member)->Get(target);
    default:
        return BoxedValue();
    }
}

bool SetEntityMemberValue(const IMember* member, Entity* entity, const BoxedValue& value)
{
    BoxedValue target = BoxedValue(entity->HandleFromThis());

    switch (member->GetMemberType())
    {
    case MemberType::Property:
        static_cast<const Property*>(member)->Set(target, value);
        return true;
    case MemberType::Field:
        static_cast<const Field*>(member)->Set(target, value);
        return true;
    default:
        return false;
    }
}

//--

EntityManager* GetEntityManagerFor(const Entity& entity)
{
    Scene* scene = entity.GetScene();

    return scene ? scene->GetEntityManager() : nullptr;
}

LayerOverridesComponent* TryGetComponent(const Entity& entity)
{
    EntityManager* entityManager = GetEntityManagerFor(entity);

    if (!entityManager)
    {
        return nullptr;
    }

    return entityManager->TryGetComponent<LayerOverridesComponent>(const_cast<const Entity*>(&entity));
}

} // anonymous namespace
} // namespace Helpers

#pragma region Queries

Name LayerOverrideSystem::GetAppliedOverrideLayer(const Entity* entity) const
{
    if (!entity)
    {
        return Name::Invalid();
    }

    const LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return Name::Invalid();
    }

    return component->appliedLayer;
}

Array<Name> LayerOverrideSystem::GetSetLayerNames(const Entity* entity) const
{
    Array<Name> layerNames;

    if (!entity)
    {
        return layerNames;
    }

    const LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return layerNames;
    }

    layerNames.Reserve(component->sets.Size());

    for (const EntityLayerOverrideSet& set : component->sets)
    {
        layerNames.PushBack(set.layerName);
    }

    return layerNames;
}

bool LayerOverrideSystem::HasLayerOverrideSet(const Entity* entity, Name layerName) const
{
    if (!entity)
    {
        return false;
    }

    const LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    return Helpers::HasLayerOverrideSet(component->sets, layerName);
}

bool LayerOverrideSystem::IsPropertyOverriddenInLayer(const Entity* entity, Name layerName, Name propertyName) const
{
    if (!entity)
    {
        return false;
    }

    const LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    return Helpers::IsPropertyOverriddenInLayer(component->sets, layerName, propertyName);
}

bool LayerOverrideSystem::GetLayerOverrideValue(const Entity* entity, Name layerName, Name propertyName, BoxedValue& outValue) const
{
    if (!entity)
    {
        return false;
    }

    const LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    return Helpers::GetLayerOverrideValue(component->sets, layerName, propertyName, outValue);
}

bool LayerOverrideSystem::GetLayerOverrideBaseValue(const Entity* entity, Name layerName, Name propertyName, BoxedValue& outValue) const
{
    if (!entity)
    {
        return false;
    }

    const LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        // Fall through to the live property value
        const IMember* member = Helpers::ResolveOverridableMember(entity->InstanceClass(), propertyName);

        if (!member)
        {
            return false;
        }

        outValue = Helpers::GetEntityMemberValue(member, const_cast<Entity*>(entity));

        return true;
    }

    if (component->appliedLayer == layerName)
    {
        for (const Pair<Name, BoxedValue>& snapshot : component->baseSnapshot)
        {
            if (snapshot.first == propertyName)
            {
                outValue = snapshot.second;

                return true;
            }
        }

        return false;
    }

    const IMember* member = Helpers::ResolveOverridableMember(entity->InstanceClass(), propertyName);

    if (!member)
    {
        return false;
    }

    outValue = Helpers::GetEntityMemberValue(member, const_cast<Entity*>(entity));

    return true;
}

#pragma endregion Queries

#pragma region Editing

bool LayerOverrideSystem::AddLayerOverrideSet(Entity* entity, Name layerName)
{
    if (!entity || !layerName)
    {
        return false;
    }

    EntityManager* entityManager = Helpers::GetEntityManagerFor(*entity);

    if (!entityManager)
    {
        return false;
    }

    LayerOverridesComponent* component = entityManager->TryGetComponent<LayerOverridesComponent>(entity);

    if (!component)
    {
        LayerOverridesComponent newComponent;

        component = &entityManager->AddComponent<LayerOverridesComponent>(entity, std::move(newComponent));
    }

    if (Helpers::HasLayerOverrideSet(component->sets, layerName))
    {
        return false;
    }

    EntityLayerOverrideSet set;
    set.layerName = layerName;

    component->sets.PushBack(std::move(set));

    entity->MarkDirty();

    return true;
}

bool LayerOverrideSystem::RemoveLayerOverrideSet(Entity* entity, Name layerName)
{
    if (!entity)
    {
        return false;
    }

    LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    if (component->appliedLayer == layerName)
    {
        RevertOverrides(entity);
    }

    for (size_t i = 0; i < component->sets.Size(); i++)
    {
        if (component->sets[i].layerName == layerName)
        {
            component->sets.EraseAt(i);

            entity->MarkDirty();

            return true;
        }
    }

    return false;
}

bool LayerOverrideSystem::SetLayerOverrideValue(Entity* entity, Name layerName, Name propertyName, BoxedValue value)
{
    if (!entity)
    {
        return false;
    }

    LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    EntityLayerOverrideSet* set = Helpers::FindLayerOverrideSet(component->sets, layerName);

    if (!set)
    {
        return false;
    }

    if (!Helpers::ResolveOverridableMember(entity->InstanceClass(), propertyName))
    {
        return false;
    }

    const bool isApplied = component->appliedLayer == layerName;

    for (LayerPropertyOverride& overrideEntry : set->propertyOverrides)
    {
        if (overrideEntry.property == propertyName)
        {
            overrideEntry.value = std::move(value);

            if (isApplied)
            {
                if (const IMember* member = Helpers::ResolveOverridableMember(entity->InstanceClass(), propertyName))
                {
                    Helpers::SetEntityMemberValue(member, entity, overrideEntry.value);
                }
            }

            entity->MarkDirty();

            return true;
        }
    }

    LayerPropertyOverride overrideEntry;
    overrideEntry.property = propertyName;
    overrideEntry.value = std::move(value);

    set->propertyOverrides.PushBack(std::move(overrideEntry));

    if (isApplied)
    {
        if (const IMember* member = Helpers::ResolveOverridableMember(entity->InstanceClass(), propertyName))
        {
            Helpers::SetEntityMemberValue(member, entity, set->propertyOverrides.Back().value);
        }
    }

    entity->MarkDirty();

    return true;
}

bool LayerOverrideSystem::RemoveLayerOverrideValue(Entity* entity, Name layerName, Name propertyName)
{
    if (!entity)
    {
        return false;
    }

    LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    EntityLayerOverrideSet* set = Helpers::FindLayerOverrideSet(component->sets, layerName);

    if (!set)
    {
        return false;
    }

    for (size_t i = 0; i < set->propertyOverrides.Size(); i++)
    {
        if (set->propertyOverrides[i].property == propertyName)
        {
            const bool isApplied = component->appliedLayer == layerName;

            set->propertyOverrides.EraseAt(i);

            if (isApplied)
            {
                // Restore the base value for this property
                const Class* cls = entity->InstanceClass();

                for (const Pair<Name, BoxedValue>& snapshot : component->baseSnapshot)
                {
                    if (snapshot.first != propertyName)
                    {
                        continue;
                    }

                    if (const IMember* member = Helpers::ResolveOverridableMember(cls, propertyName))
                    {
                        Helpers::SetEntityMemberValue(member, entity, snapshot.second);
                    }

                    break;
                }
            }

            entity->MarkDirty();

            return true;
        }
    }

    return false;
}

bool LayerOverrideSystem::SetLayerOverrideBaseValue(Entity* entity, Name propertyName, BoxedValue value)
{
    if (!entity)
    {
        return false;
    }

    const IMember* member = Helpers::ResolveOverridableMember(entity->InstanceClass(), propertyName);

    if (!member)
    {
        return false;
    }

    if (!Helpers::SetEntityMemberValue(member, entity, value))
    {
        return false;
    }

    // Keep the applied layer's base snapshot in sync, so a revert returns to the edited base
    if (LayerOverridesComponent* component = Helpers::TryGetComponent(*entity); component && component->appliedLayer.IsValid())
    {
        for (Pair<Name, BoxedValue>& snapshot : component->baseSnapshot)
        {
            if (snapshot.first == propertyName)
            {
                snapshot.second = std::move(value);

                break;
            }
        }
    }

    entity->SetNeedsRenderProxyUpdate();
    entity->MarkDirty();

    return true;
}

#pragma endregion Editing

#pragma region Apply / revert

void LayerOverrideSystem::ApplyOverrides(Entity* entity, Name layerName)
{
    if (!entity)
    {
        return;
    }

    LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return;
    }

    if (component->appliedLayer == layerName)
    {
        return;
    }

    // Restore base values before applying the new set
    RevertOverrides(entity);

    const EntityLayerOverrideSet* set = Helpers::FindLayerOverrideSet(component->sets, layerName);

    if (!set || set->propertyOverrides.Empty())
    {
        return;
    }

    const Class* cls = entity->InstanceClass();

    Array<Pair<Name, BoxedValue>> baseSnapshot;
    uint32 numApplied = 0;

    for (const LayerPropertyOverride& overrideEntry : set->propertyOverrides)
    {
        const IMember* member = Helpers::ResolveOverridableMember(cls, overrideEntry.property);

        if (!member)
        {
            HYP_LOG(Entity, Warning, "Cannot apply layer override: Entity {} has no settable property '{}'",
                entity->GetName(), overrideEntry.property);

            continue;
        }

        // Snapshot the base value (first occurrence wins; duplicates apply last-wins)
        bool snapshotted = false;

        for (const Pair<Name, BoxedValue>& snapshot : baseSnapshot)
        {
            if (snapshot.first == overrideEntry.property)
            {
                snapshotted = true;

                break;
            }
        }

        if (!snapshotted)
        {
            baseSnapshot.PushBack({ overrideEntry.property, Helpers::GetEntityMemberValue(member, entity) });
        }

        if (!Helpers::SetEntityMemberValue(member, entity, overrideEntry.value))
        {
            HYP_LOG(Entity, Warning, "Failed to apply layer override '{}' on Entity {}", overrideEntry.property, entity->GetName());

            continue;
        }

        ++numApplied;
    }

    component->baseSnapshot = std::move(baseSnapshot);
    component->appliedLayer = layerName;

    HYP_LOG(Entity, Info, "Applied {} layer override(s) for layer '{}' on Entity '{}'", numApplied, layerName, entity->GetName());

    entity->SetNeedsRenderProxyUpdate();
    entity->MarkDirty();
}

void LayerOverrideSystem::RevertOverrides(Entity* entity)
{
    if (!entity)
    {
        return;
    }

    LayerOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component || !component->appliedLayer)
    {
        return;
    }

    const Class* cls = entity->InstanceClass();

    for (const Pair<Name, BoxedValue>& snapshot : component->baseSnapshot)
    {
        if (const IMember* member = Helpers::ResolveOverridableMember(cls, snapshot.first))
        {
            Helpers::SetEntityMemberValue(member, entity, snapshot.second);
        }
    }

    component->baseSnapshot.Clear();
    component->appliedLayer = Name::Invalid();

    entity->SetNeedsRenderProxyUpdate();
    entity->MarkDirty();
}

void LayerOverrideSystem::ApplyActive()
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    const Name activeLayerName = world->GetActiveLayerName();

    uint32 numEntitiesWithOverrides = 0;

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        for (auto [entity, component] : scene->GetEntityManager()->GetEntitySet<LayerOverridesComponent>().GetScopedView(GetComponentInfos()))
        {
            ++numEntitiesWithOverrides;

            ApplyOverrides(entity, activeLayerName);
        }
    }

    HYP_LOG(Scene, Info, "Applying layer overrides for active layer '{}' across {} entit(ies) with override sets",
        activeLayerName, numEntitiesWithOverrides);
}

void LayerOverrideSystem::RevertAll()
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        for (auto [entity, component] : scene->GetEntityManager()->GetEntitySet<LayerOverridesComponent>().GetScopedView(GetComponentInfos()))
        {
            RevertOverrides(entity);
        }
    }
}

void LayerOverrideSystem::OnEntityAddedToWorld(Entity* entity)
{
    World* world = GetWorld();

    if (!world || entity->GetWorld() != world)
    {
        return;
    }

    if (!Helpers::TryGetComponent(*entity))
    {
        return;
    }

    ApplyOverrides(entity, world->GetActiveLayerName());
}

#pragma endregion Apply / revert

#pragma region SystemBase

void LayerOverrideSystem::OnEntityAdded(Entity* entity)
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    // Only apply if the Entity is part of this World; entities that are still being parsed
    // or constructed outside of the World are handled by OnEntityAddedToWorld.
    if (entity->GetWorld() != world)
    {
        return;
    }

    if (!Helpers::TryGetComponent(*entity))
    {
        return;
    }

    ApplyOverrides(entity, world->GetActiveLayerName());
}

void LayerOverrideSystem::OnEntityRemoved(Entity* entity)
{
    RevertOverrides(entity);
}

#pragma endregion SystemBase

} // namespace Hyperion
