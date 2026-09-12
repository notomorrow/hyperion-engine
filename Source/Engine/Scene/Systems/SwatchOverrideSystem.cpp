/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/SwatchOverrideSystem.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Swatch.hpp>

#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Field.hpp>

#include <SwatchOverrideSystem.generated.inl>

namespace Hyperion {

namespace Helpers
{
namespace
{

inline EntitySwatchOverrideSet* FindSwatchOverrideSet(Array<EntitySwatchOverrideSet>& sets, Name swatchName)
{
    for (EntitySwatchOverrideSet& set : sets)
    {
        if (set.swatchName == swatchName)
        {
            return &set;
        }
    }

    return nullptr;
}

inline const EntitySwatchOverrideSet* FindSwatchOverrideSet(const Array<EntitySwatchOverrideSet>& sets, Name swatchName)
{
    for (const EntitySwatchOverrideSet& set : sets)
    {
        if (set.swatchName == swatchName)
        {
            return &set;
        }
    }

    return nullptr;
}

inline bool HasSwatchOverrideSet(const Array<EntitySwatchOverrideSet>& sets, Name swatchName)
{
    return FindSwatchOverrideSet(sets, swatchName) != nullptr;
}

inline bool IsPropertyOverriddenInSwatch(const Array<EntitySwatchOverrideSet>& sets, Name swatchName, Name propertyName)
{
    const EntitySwatchOverrideSet* set = FindSwatchOverrideSet(sets, swatchName);

    if (!set)
    {
        return false;
    }

    for (const SwatchPropertyOverride& overrideEntry : set->propertyOverrides)
    {
        if (overrideEntry.property == propertyName)
        {
            return true;
        }
    }

    return false;
}

inline bool GetSwatchOverrideValue(const Array<EntitySwatchOverrideSet>& sets, Name swatchName, Name propertyName, BoxedValue& outValue)
{
    const EntitySwatchOverrideSet* set = FindSwatchOverrideSet(sets, swatchName);

    if (!set)
    {
        return false;
    }

    for (const SwatchPropertyOverride& overrideEntry : set->propertyOverrides)
    {
        if (overrideEntry.property == propertyName)
        {
            outValue = overrideEntry.value;

            return true;
        }
    }

    return false;
}

///Reflection

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

    if (member->GetAttribute(Attributes::g_attrNoSwatchOverride).IsValid())
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

////////////////////

EntityManager* GetEntityManagerFor(const Entity& entity)
{
    Scene* scene = entity.GetScene();

    return scene ? scene->GetEntityManager() : nullptr;
}

SwatchOverridesComponent* TryGetComponent(const Entity& entity)
{
    EntityManager* entityManager = GetEntityManagerFor(entity);

    if (!entityManager)
    {
        return nullptr;
    }

    return entityManager->TryGetComponent<SwatchOverridesComponent>(const_cast<const Entity*>(&entity));
}

void SnapshotBaseValueIfAbsent(Array<Pair<Name, BoxedValue>>& baseSnapshot, const IMember* member, Entity* entity, Name propertyName)
{
    for (const Pair<Name, BoxedValue>& snapshot : baseSnapshot)
    {
        if (snapshot.first == propertyName)
        {
            return;
        }
    }

    baseSnapshot.PushBack({ propertyName, GetEntityMemberValue(member, entity) });
}

bool BoxesEqual(const BoxedValue& a, const BoxedValue& b)
{
    return a.value == b.value;
}

bool GetEntityTrueBaseValue(Entity* entity, Name propertyName, BoxedValue& outValue)
{
    SwatchOverridesComponent* component = TryGetComponent(*entity);

    if (component && component->appliedSwatch.IsValid())
    {
        for (const Pair<Name, BoxedValue>& snapshot : component->baseSnapshot)
        {
            if (snapshot.first == propertyName)
            {
                outValue = snapshot.second;

                return true;
            }
        }
    }

    const IMember* member = ResolveOverridableMember(entity->InstanceClass(), propertyName);

    if (!member)
    {
        return false;
    }

    outValue = GetEntityMemberValue(member, entity);

    return true;
}

} // anonymous namespace
} // namespace Helpers

#pragma region Queries

Name SwatchOverrideSystem::GetAppliedOverrideSwatch(const Entity* entity) const
{
    if (!entity)
    {
        return Name::Invalid();
    }

    const SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return Name::Invalid();
    }

    return component->appliedSwatch;
}

Array<Name> SwatchOverrideSystem::GetSetSwatchNames(const Entity* entity) const
{
    Array<Name> swatchNames;

    if (!entity)
    {
        return swatchNames;
    }

    const SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return swatchNames;
    }

    swatchNames.Reserve(component->sets.Size());

    for (const EntitySwatchOverrideSet& set : component->sets)
    {
        if (IsDefaultSwatch(set.swatchName))
        {
            continue;
        }

        swatchNames.PushBack(set.swatchName);
    }

    return swatchNames;
}

bool SwatchOverrideSystem::HasSwatchOverrideSet(const Entity* entity, Name swatchName) const
{
    if (!entity || IsDefaultSwatch(swatchName))
    {
        return false;
    }

    const SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    return Helpers::HasSwatchOverrideSet(component->sets, swatchName);
}

bool SwatchOverrideSystem::IsPropertyOverriddenInSwatch(const Entity* entity, Name swatchName, Name propertyName) const
{
    if (!entity || IsDefaultSwatch(swatchName))
    {
        return false;
    }

    const SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    return Helpers::IsPropertyOverriddenInSwatch(component->sets, swatchName, propertyName);
}

bool SwatchOverrideSystem::GetSwatchOverrideValue(const Entity* entity, Name swatchName, Name propertyName, BoxedValue& outValue) const
{
    if (!entity || IsDefaultSwatch(swatchName))
    {
        return false;
    }

    const SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    return Helpers::GetSwatchOverrideValue(component->sets, swatchName, propertyName, outValue);
}

bool SwatchOverrideSystem::GetSwatchOverrideBaseValue(const Entity* entity, Name swatchName, Name propertyName, BoxedValue& outValue) const
{
    if (!entity)
    {
        return false;
    }

    if (IsDefaultSwatch(swatchName))
    {
        return Helpers::GetEntityTrueBaseValue(const_cast<Entity*>(entity), propertyName, outValue);
    }

    const SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

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

    if (component->appliedSwatch == swatchName)
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

bool SwatchOverrideSystem::HasAnyOverriddenProperty(const Entity* entity, Name swatchName) const
{
    if (!entity || !swatchName || IsDefaultSwatch(swatchName))
    {
        return false;
    }

    const SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    const EntitySwatchOverrideSet* set = Helpers::FindSwatchOverrideSet(component->sets, swatchName);

    return set && !set->propertyOverrides.Empty();
}

Array<Pair<Name, BoxedValue>> SwatchOverrideSystem::GetSwatchOverrideEntries(const Entity* entity, Name swatchName) const
{
    Array<Pair<Name, BoxedValue>> entries;

    if (!entity || !swatchName || IsDefaultSwatch(swatchName))
    {
        return entries;
    }

    const SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return entries;
    }

    const EntitySwatchOverrideSet* set = Helpers::FindSwatchOverrideSet(component->sets, swatchName);

    if (!set)
    {
        return entries;
    }

    for (const SwatchPropertyOverride& overrideEntry : set->propertyOverrides)
    {
        entries.PushBack({ overrideEntry.property, overrideEntry.value });
    }

    return entries;
}

#pragma endregion Queries

#pragma region Editing

bool SwatchOverrideSystem::AddSwatchOverrideSet(Entity* entity, Name swatchName)
{
    // The Default swatch has no override set - editing it writes the base values directly
    if (!entity || !swatchName || IsDefaultSwatch(swatchName))
    {
        return false;
    }

    EntityManager* entityManager = Helpers::GetEntityManagerFor(*entity);

    if (!entityManager)
    {
        return false;
    }

    SwatchOverridesComponent* component = entityManager->TryGetComponent<SwatchOverridesComponent>(entity);

    if (!component)
    {
        SwatchOverridesComponent newComponent;

        component = &entityManager->AddComponent<SwatchOverridesComponent>(entity, std::move(newComponent));
    }

    if (Helpers::HasSwatchOverrideSet(component->sets, swatchName))
    {
        return false;
    }

    EntitySwatchOverrideSet set;
    set.swatchName = swatchName;

    component->sets.PushBack(std::move(set));

    entity->MarkDirty();

    return true;
}

bool SwatchOverrideSystem::RemoveSwatchOverrideSet(Entity* entity, Name swatchName)
{
    if (!entity || IsDefaultSwatch(swatchName))
    {
        return false;
    }

    SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    if (component->appliedSwatch == swatchName)
    {
        RevertOverrides(entity);
    }

    for (size_t i = 0; i < component->sets.Size(); i++)
    {
        if (component->sets[i].swatchName == swatchName)
        {
            component->sets.EraseAt(i);

            entity->MarkDirty();

            return true;
        }
    }

    return false;
}

bool SwatchOverrideSystem::SetSwatchOverrideValue(Entity* entity, Name swatchName, Name propertyName, BoxedValue value)
{
    if (!entity)
    {
        return false;
    }

    // Writing to the Default swatch is writing the base value
    if (IsDefaultSwatch(swatchName))
    {
        return SetSwatchOverrideBaseValue(entity, propertyName, std::move(value));
    }

    SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    EntitySwatchOverrideSet* set = Helpers::FindSwatchOverrideSet(component->sets, swatchName);

    if (!set)
    {
        return false;
    }

    if (!Helpers::ResolveOverridableMember(entity->InstanceClass(), propertyName))
    {
        return false;
    }

    const bool isApplied = component->appliedSwatch == swatchName;

    for (SwatchPropertyOverride& overrideEntry : set->propertyOverrides)
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

    SwatchPropertyOverride overrideEntry;
    overrideEntry.property = propertyName;
    overrideEntry.value = std::move(value);

    set->propertyOverrides.PushBack(std::move(overrideEntry));

    if (isApplied)
    {
        if (const IMember* member = Helpers::ResolveOverridableMember(entity->InstanceClass(), propertyName))
        {
            // The property wasn't in the set when the swatch was applied, so it has no snapshot entry yet.
            // Without one, RevertOverrides would leave this override sitting in the base value.
            Helpers::SnapshotBaseValueIfAbsent(component->baseSnapshot, member, entity, propertyName);

            Helpers::SetEntityMemberValue(member, entity, set->propertyOverrides.Back().value);
        }
    }

    entity->MarkDirty();

    return true;
}

bool SwatchOverrideSystem::RemoveSwatchOverrideValue(Entity* entity, Name swatchName, Name propertyName)
{
    if (!entity || IsDefaultSwatch(swatchName))
    {
        return false;
    }

    SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return false;
    }

    EntitySwatchOverrideSet* set = Helpers::FindSwatchOverrideSet(component->sets, swatchName);

    if (!set)
    {
        return false;
    }

    for (size_t i = 0; i < set->propertyOverrides.Size(); i++)
    {
        if (set->propertyOverrides[i].property == propertyName)
        {
            const bool isApplied = component->appliedSwatch == swatchName;

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

bool SwatchOverrideSystem::SetSwatchOverrideBaseValue(Entity* entity, Name propertyName, BoxedValue value)
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

    // Keep the applied swatch's base snapshot in sync, so a revert returns to the edited base
    if (SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity); component && component->appliedSwatch.IsValid())
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

#pragma region Copy

Array<SwatchPropertyCopyEntry> SwatchOverrideSystem::BuildSwatchPropertyCopyPlan(Entity* entity, Name sourceSwatch, Name targetSwatch) const
{
    Array<SwatchPropertyCopyEntry> plan;

    // Copying from/to Default is copying from/to the base values
    if (IsDefaultSwatch(sourceSwatch))
    {
        sourceSwatch = Name::Invalid();
    }

    if (IsDefaultSwatch(targetSwatch))
    {
        targetSwatch = Name::Invalid();
    }

    if (!entity || (!sourceSwatch.IsValid() && !targetSwatch.IsValid()))
    {
        return plan; // base -> base, nothing to do
    }

    const SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return plan;
    }

    const EntitySwatchOverrideSet* sourceSet = sourceSwatch.IsValid()
        ? Helpers::FindSwatchOverrideSet(component->sets, sourceSwatch)
        : nullptr;
    const EntitySwatchOverrideSet* targetSet = targetSwatch.IsValid()
        ? Helpers::FindSwatchOverrideSet(component->sets, targetSwatch)
        : nullptr;

    if (!sourceSet && !targetSet)
    {
        return plan;
    }

    // Only properties overridden on either side can differ between source and target; for
    // everything else both sides evaluate to the base value
    Array<Name> propertyNames;

    auto addPropertyName = [&propertyNames](Name propertyName)
    {
        for (const Name& existing : propertyNames)
        {
            if (existing == propertyName)
            {
                return;
            }
        }

        propertyNames.PushBack(propertyName);
    };

    if (sourceSet)
    {
        for (const SwatchPropertyOverride& overrideEntry : sourceSet->propertyOverrides)
        {
            addPropertyName(overrideEntry.property);
        }
    }

    if (targetSet)
    {
        for (const SwatchPropertyOverride& overrideEntry : targetSet->propertyOverrides)
        {
            addPropertyName(overrideEntry.property);
        }
    }

    for (const Name& propertyName : propertyNames)
    {
        BoxedValue baseValue;

        if (!Helpers::GetEntityTrueBaseValue(const_cast<Entity *>(entity), propertyName, baseValue))
        {
            continue;
        }

        BoxedValue sourceValue = baseValue;

        if (sourceSet)
        {
            for (const SwatchPropertyOverride& overrideEntry : sourceSet->propertyOverrides)
            {
                if (overrideEntry.property == propertyName)
                {
                    sourceValue = overrideEntry.value;

                    break;
                }
            }
        }

        BoxedValue targetOverrideValue;
        bool targetOverridden = false;

        if (targetSet)
        {
            for (const SwatchPropertyOverride& overrideEntry : targetSet->propertyOverrides)
            {
                if (overrideEntry.property == propertyName)
                {
                    targetOverrideValue = overrideEntry.value;
                    targetOverridden = true;

                    break;
                }
            }
        }

        if (Helpers::BoxesEqual(sourceValue, baseValue))
        {
            // Source matches base here: the only change a copy can need is pruning the
            // target's redundant override of this property
            if (targetSwatch.IsValid() && targetOverridden)
            {
                SwatchPropertyCopyEntry entry;
                entry.propertyName = propertyName;
                entry.op = SwatchPropertyCopyOp::RemoveOverride;
                entry.oldValue = targetOverrideValue;
                entry.hasOldValue = true;

                plan.PushBack(std::move(entry));
            }

            continue;
        }

        if (!targetSwatch.IsValid())
        {
            // Copying into base: write the source value as the new base
            SwatchPropertyCopyEntry entry;
            entry.propertyName = propertyName;
            entry.op = SwatchPropertyCopyOp::SetBase;
            entry.newValue = sourceValue;
            entry.oldValue = baseValue;
            entry.hasOldValue = true;

            plan.PushBack(std::move(entry));

            continue;
        }

        if (targetOverridden && Helpers::BoxesEqual(targetOverrideValue, sourceValue))
        {
            continue; // target already matches the source
        }

        SwatchPropertyCopyEntry entry;
        entry.propertyName = propertyName;
        entry.op = SwatchPropertyCopyOp::WriteOverride;
        entry.newValue = sourceValue;
        entry.oldValue = targetOverrideValue;
        entry.hasOldValue = targetOverridden;

        plan.PushBack(std::move(entry));
    }

    return plan;
}

void SwatchOverrideSystem::ApplySwatchPropertyCopyEntries(Entity* entity, Name targetSwatch, const Array<SwatchPropertyCopyEntry>& entries, bool applyNewState)
{
    if (!entity)
    {
        return;
    }

    for (const SwatchPropertyCopyEntry& entry : entries)
    {
        switch (entry.op)
        {
        case SwatchPropertyCopyOp::SetBase:
        {
            if (!applyNewState && !entry.hasOldValue)
            {
                break;
            }

            SetSwatchOverrideBaseValue(entity, entry.propertyName, applyNewState ? entry.newValue : entry.oldValue);

            break;
        }

        case SwatchPropertyCopyOp::WriteOverride:
        {
            if (!targetSwatch.IsValid())
            {
                break;
            }

            if (applyNewState)
            {
                if (!HasSwatchOverrideSet(entity, targetSwatch))
                {
                    AddSwatchOverrideSet(entity, targetSwatch);
                }

                SetSwatchOverrideValue(entity, targetSwatch, entry.propertyName, entry.newValue);
            }
            else if (entry.hasOldValue)
            {
                if (!HasSwatchOverrideSet(entity, targetSwatch))
                {
                    AddSwatchOverrideSet(entity, targetSwatch);
                }

                SetSwatchOverrideValue(entity, targetSwatch, entry.propertyName, entry.oldValue);
            }
            else
            {
                RemoveSwatchOverrideValue(entity, targetSwatch, entry.propertyName);
            }

            break;
        }

        case SwatchPropertyCopyOp::RemoveOverride:
        {
            if (!targetSwatch.IsValid())
            {
                break;
            }

            if (applyNewState)
            {
                RemoveSwatchOverrideValue(entity, targetSwatch, entry.propertyName);
            }
            else if (entry.hasOldValue)
            {
                if (!HasSwatchOverrideSet(entity, targetSwatch))
                {
                    AddSwatchOverrideSet(entity, targetSwatch);
                }

                SetSwatchOverrideValue(entity, targetSwatch, entry.propertyName, entry.oldValue);
            }

            break;
        }
        }
    }
}

#pragma endregion Copy

#pragma region Apply / revert

void SwatchOverrideSystem::ApplyOverrides(Entity* entity, Name swatchName)
{
    if (!entity)
    {
        return;
    }

    // The Default swatch holds no overrides - it is the entity's base state
    if (IsDefaultSwatch(swatchName))
    {
        RevertOverrides(entity);

        return;
    }

    SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component)
    {
        return;
    }

    if (component->appliedSwatch == swatchName)
    {
        return;
    }

    // Restore base values before applying the new set
    RevertOverrides(entity);

    const EntitySwatchOverrideSet* set = Helpers::FindSwatchOverrideSet(component->sets, swatchName);

    if (!set || set->propertyOverrides.Empty())
    {
        return;
    }

    const Class* cls = entity->InstanceClass();

    Array<Pair<Name, BoxedValue>> baseSnapshot;
    uint32 numApplied = 0;

    for (const SwatchPropertyOverride& overrideEntry : set->propertyOverrides)
    {
        const IMember* member = Helpers::ResolveOverridableMember(cls, overrideEntry.property);

        if (!member)
        {
            HYP_LOG(Entity, Warning, "Cannot apply swatch override: Entity {} has no settable property '{}'",
                entity->GetName(), overrideEntry.property);

            continue;
        }

        // Snapshot the base value (first occurrence wins; duplicates apply last-wins)
        Helpers::SnapshotBaseValueIfAbsent(baseSnapshot, member, entity, overrideEntry.property);

        if (!Helpers::SetEntityMemberValue(member, entity, overrideEntry.value))
        {
            HYP_LOG(Entity, Warning, "Failed to apply swatch override '{}' on Entity {}", overrideEntry.property, entity->GetName());

            continue;
        }

        ++numApplied;
    }

    component->baseSnapshot = std::move(baseSnapshot);
    component->appliedSwatch = swatchName;

    HYP_LOG(Entity, Info, "Applied {} swatch override(s) for swatch '{}' on Entity '{}'", numApplied, swatchName, entity->GetName());

    entity->SetNeedsRenderProxyUpdate();
    entity->MarkDirty();
}

void SwatchOverrideSystem::RevertOverrides(Entity* entity)
{
    if (!entity)
    {
        return;
    }

    SwatchOverridesComponent* component = Helpers::TryGetComponent(*entity);

    if (!component || !component->appliedSwatch)
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
    component->appliedSwatch = Name::Invalid();

    entity->SetNeedsRenderProxyUpdate();
    entity->MarkDirty();
}

void SwatchOverrideSystem::ApplyActive()
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    const Name activeSwatchName = world->GetActiveSwatchName();

    uint32 numEntitiesWithOverrides = 0;

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        for (auto [entity, component] : scene->GetEntityManager()->GetEntitySet<SwatchOverridesComponent>().GetScopedView(GetComponentInfos()))
        {
            ++numEntitiesWithOverrides;

            ApplyOverrides(entity, activeSwatchName);
        }
    }

    HYP_LOG(Scene, Info, "Applying swatch overrides for active swatch '{}' across {} entit(ies) with override sets",
        activeSwatchName, numEntitiesWithOverrides);
}

void SwatchOverrideSystem::RevertAll()
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

        for (auto [entity, component] : scene->GetEntityManager()->GetEntitySet<SwatchOverridesComponent>().GetScopedView(GetComponentInfos()))
        {
            RevertOverrides(entity);
        }
    }
}

void SwatchOverrideSystem::OnEntityAddedToWorld(Entity* entity)
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

    ApplyOverrides(entity, world->GetActiveSwatchName());
}

#pragma endregion Apply / revert

#pragma region SystemBase

void SwatchOverrideSystem::OnEntityAdded(Entity* entity)
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

    ApplyOverrides(entity, world->GetActiveSwatchName());
}

void SwatchOverrideSystem::OnEntityRemoved(Entity* entity)
{
    RevertOverrides(entity);
}

#pragma endregion SystemBase

} // namespace Hyperion
