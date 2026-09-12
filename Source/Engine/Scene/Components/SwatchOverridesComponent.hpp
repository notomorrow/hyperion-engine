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

struct SwatchPropertyOverride
{
    Name property;
    BoxedValue value;
};

struct EntitySwatchOverrideSet
{
    Name swatchName;
    Array<SwatchPropertyOverride> propertyOverrides;
};

HYP_STRUCT(Component, NoScriptBindings, Serialize = false, Editor = false, Replicated = false, Label = "Swatch Overrides", Description = "Per-swatch property overrides for an entity.")
struct SwatchOverridesComponent
{
    HYP_STRUCT_BODY(SwatchOverridesComponent);

    HYP_FIELD(Transient)
    Array<EntitySwatchOverrideSet> sets;

    /// managed by SwatchOverrideSystem  --

    // Name of the swatch active
    HYP_FIELD(Transient)
    Name appliedSwatch;

    // Base values of all overridden properties, captured when a set is applied
    /// @TODO: Just reload the Entity HMF data; apply it that way... Less mem usage / interning.
    HYP_FIELD(Transient)
    Array<Pair<Name, BoxedValue>> baseSnapshot;
};

////////////////////

} // namespace Hyperion
