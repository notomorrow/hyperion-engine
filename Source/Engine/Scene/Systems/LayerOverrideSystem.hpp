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

    //-- Editing

    bool AddLayerOverrideSet(Entity* entity, Name layerName);
    bool RemoveLayerOverrideSet(Entity* entity, Name layerName);

    bool SetLayerOverrideValue(Entity* entity, Name layerName, Name propertyName, BoxedValue value);

    bool RemoveLayerOverrideValue(Entity* entity, Name layerName, Name propertyName);
    bool SetLayerOverrideBaseValue(Entity* entity, Name propertyName, BoxedValue value);

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
