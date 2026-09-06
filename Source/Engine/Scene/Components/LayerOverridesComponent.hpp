/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Name/Name.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Reflection/ObjectMacros.hpp>
#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Utilities/Pair.hpp>

namespace Hyperion {

struct LayerPropertyOverride
{
    Name property;
    BoxedValue value;
};

struct EntityLayerOverrideSet
{
    Name layerName;
    Array<LayerPropertyOverride> propertyOverrides;
};

HYP_STRUCT(Component, NoScriptBindings, Serialize = false, Editor = false, Replicated = false, Label = "Layer Overrides", Description = "Per-layer property overrides for an entity.")
struct LayerOverridesComponent
{
    HYP_STRUCT_BODY(LayerOverridesComponent);

    HYP_FIELD(Transient)
    Array<EntityLayerOverrideSet> sets;

    //--  managed by LayerOverrideSystem  --

    // Name of the layer active
    HYP_FIELD(Transient)
    Name appliedLayer;

    // Base values of all overridden properties, captured when a set is applied
    /// @TODO: Just reload the Entity HMF data; apply it that way... Less mem usage / interning.
    HYP_FIELD(Transient)
    Array<Pair<Name, BoxedValue>> baseSnapshot;
};

//--

} // namespace Hyperion
