/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>

#include <Scene/Components/LayerOverridesComponent.hpp>

namespace Hyperion {

class Entity;

enum class LayerPropertyCopyOp
{
    SetBase,        // target is base: write the source value as the new base
    WriteOverride,  // target is a layer: write the value into its override set
    RemoveOverride  // target is a layer: prune the (redundant) override entry
};

struct LayerPropertyCopyEntry
{
    Name propertyName;
    LayerPropertyCopyOp op = LayerPropertyCopyOp::SetBase;
    BoxedValue newValue;    // SetBase / WriteOverride: the value taken from the source
    BoxedValue oldValue;    // previous state, captured for undo
    bool hasOldValue = false;
};

HYP_CLASS(NoScriptBindings, Serialize = false)
class LayerOverrideSystem final : public SystemBase
{
    HYP_OBJECT_BODY(LayerOverrideSystem);

public:
    virtual ~LayerOverrideSystem() override = default;

    //-- Queries

    Name GetAppliedOverrideLayer(const Entity* entity) const;

    Array<Name> GetSetLayerNames(const Entity* entity) const;

    bool HasLayerOverrideSet(const Entity* entity, Name layerName) const;

    bool IsPropertyOverriddenInLayer(const Entity* entity, Name layerName, Name propertyName) const;

    bool GetLayerOverrideValue(const Entity* entity, Name layerName, Name propertyName, BoxedValue& outValue) const;
    bool GetLayerOverrideBaseValue(const Entity* entity, Name layerName, Name propertyName, BoxedValue& outValue) const;

    bool HasAnyOverriddenProperty(const Entity* entity, Name layerName) const;
    Array<Pair<Name, BoxedValue>> GetLayerOverrideEntries(const Entity* entity, Name layerName) const;

    //-- Editing

    bool AddLayerOverrideSet(Entity* entity, Name layerName);
    bool RemoveLayerOverrideSet(Entity* entity, Name layerName);

    bool SetLayerOverrideValue(Entity* entity, Name layerName, Name propertyName, BoxedValue value);

    bool RemoveLayerOverrideValue(Entity* entity, Name layerName, Name propertyName);
    bool SetLayerOverrideBaseValue(Entity* entity, Name propertyName, BoxedValue value);

    //-- Copy

    Array<LayerPropertyCopyEntry> BuildLayerPropertyCopyPlan(Entity* entity, Name sourceLayer, Name targetLayer) const;
    void ApplyLayerPropertyCopyEntries(Entity* entity, Name targetLayer, const Array<LayerPropertyCopyEntry>& entries, bool applyNewState);

    //-- Apply / revert

    void ApplyOverrides(Entity* entity, Name layerName);
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
            ComponentDescriptor<LayerOverridesComponent, ComponentAccess::READ_WRITE> {}
        };
    }
};

} // namespace Hyperion
