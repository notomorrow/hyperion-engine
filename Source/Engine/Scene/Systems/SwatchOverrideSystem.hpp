/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>

#include <Scene/Components/SwatchOverridesComponent.hpp>

namespace Hyperion {

class Entity;

enum class SwatchPropertyCopyOp
{
    SetBase,        // target is base: write the source value as the new base
    WriteOverride,  // target is a swatch: write the value into its override set
    RemoveOverride  // target is a swatch: prune the (redundant) override entry
};

struct SwatchPropertyCopyEntry
{
    Name propertyName;
    SwatchPropertyCopyOp op = SwatchPropertyCopyOp::SetBase;
    BoxedValue newValue;    // SetBase / WriteOverride: the value taken from the source
    BoxedValue oldValue;    // previous state, captured for undo
    bool hasOldValue = false;
};

HYP_CLASS(NoScriptBindings, Serialize = false)
class SwatchOverrideSystem final : public SystemBase
{
    HYP_OBJECT_BODY(SwatchOverrideSystem);

public:
    virtual ~SwatchOverrideSystem() override = default;

    //-- Queries

    Name GetAppliedOverrideSwatch(const Entity* entity) const;

    Array<Name> GetSetSwatchNames(const Entity* entity) const;

    bool HasSwatchOverrideSet(const Entity* entity, Name swatchName) const;

    bool IsPropertyOverriddenInSwatch(const Entity* entity, Name swatchName, Name propertyName) const;

    bool GetSwatchOverrideValue(const Entity* entity, Name swatchName, Name propertyName, BoxedValue& outValue) const;
    bool GetSwatchOverrideBaseValue(const Entity* entity, Name swatchName, Name propertyName, BoxedValue& outValue) const;

    bool HasAnyOverriddenProperty(const Entity* entity, Name swatchName) const;
    Array<Pair<Name, BoxedValue>> GetSwatchOverrideEntries(const Entity* entity, Name swatchName) const;

    //-- Editing

    bool AddSwatchOverrideSet(Entity* entity, Name swatchName);
    bool RemoveSwatchOverrideSet(Entity* entity, Name swatchName);

    bool SetSwatchOverrideValue(Entity* entity, Name swatchName, Name propertyName, BoxedValue value);

    bool RemoveSwatchOverrideValue(Entity* entity, Name swatchName, Name propertyName);
    bool SetSwatchOverrideBaseValue(Entity* entity, Name propertyName, BoxedValue value);

    //-- Copy

    Array<SwatchPropertyCopyEntry> BuildSwatchPropertyCopyPlan(Entity* entity, Name sourceSwatch, Name targetSwatch) const;
    void ApplySwatchPropertyCopyEntries(Entity* entity, Name targetSwatch, const Array<SwatchPropertyCopyEntry>& entries, bool applyNewState);

    //-- Apply / revert

    void ApplyOverrides(Entity* entity, Name swatchName);
    void RevertOverrides(Entity* entity);

    void ApplyActive();

    void RevertAll();

    void OnEntityAddedToWorld(Entity* entity);

    //-- SystemBase

    bool AllowUpdate() const override
    {
        return false;
    }

    bool RequiresSimThread() const override
    {
        return true;
    }

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override
    {
        // No-op
    }

protected:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<SwatchOverridesComponent, ComponentAccess::READ_WRITE> {}
        };
    }
};

} // namespace Hyperion
