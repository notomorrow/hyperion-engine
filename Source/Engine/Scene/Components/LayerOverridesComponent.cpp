/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Components/LayerOverridesComponent.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <LayerOverridesComponent.generated.inl>

namespace Hyperion {

//-- $LayerOverrides schema section parsing --

// The parser types each entry of the section against the owner Entity's own class schema
// (see Parser::ParseSchemaSection), so the collected values arrive fully typed.
static bool HandleLayerOverridesSection(BoxedValue& owner, Array<HMF::SchemaSectionEntry>&& entries)
{
    Entity* entity = nullptr;

    if (!owner.IsNull())
    {
        void* pointer = owner.ToRef().GetPointer();

        if (pointer != nullptr)
        {
            auto* object = reinterpret_cast<ObjectBase*>(pointer);

            if (IsA(Entity::StaticClass(), object->InstanceClass()))
            {
                entity = static_cast<Entity*>(object);
            }
        }
    }

    if (!entity)
    {
        return false;
    }

    Array<EntityLayerOverrideSet> sets;
    sets.Reserve(entries.Size());

    for (HMF::SchemaSectionEntry& entry : entries)
    {
        EntityLayerOverrideSet set;
        set.layerName = entry.key;
        set.propertyOverrides.Reserve(entry.values.Size());

        for (Pair<Name, BoxedValue>& value : entry.values)
        {
            LayerPropertyOverride overrideEntry;
            overrideEntry.property = value.first;
            overrideEntry.value = std::move(value.second);

            set.propertyOverrides.PushBack(std::move(overrideEntry));
        }

        sets.PushBack(std::move(set));
    }

    // An Entity is generally not registered with an EntityManager while its HMF data is being
    // parsed (e.g. prefab root data, or Entities referenced from a World mid-load), even though
    // it has a (detached) Scene. Stash the parsed sets on the Entity; they are written into the
    // LayerOverridesComponent once the Entity has been registered (Entity::Init).
    entity->SetPendingLayerOverrides(std::move(sets));
    entity->FlushPendingLayerOverrides();

    return true;
}

//-- DI for $LayerOverrides

static struct InitializeLayerOverridesSinks
{
    InitializeLayerOverridesSinks()
    {
        HMF::SetParseSchemaSectionFn("LayerOverrides", &HandleLayerOverridesSection);
    }
} s_installLayerOverridesSink;

} // namespace Hyperion
