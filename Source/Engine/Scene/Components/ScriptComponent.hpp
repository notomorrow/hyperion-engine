/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Utilities/EnumFlags.hpp>

#include <Scripting/Script.hpp>
#include <Scripting/ScriptObjectResource.hpp>
#include <Scripting/Asset/ScriptAsset.hpp>

#include <Core/HashCode.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Name/Name.hpp>

#include <Core/Reflection/BoxedValue.hpp>

namespace Hyperion {

namespace dotnet {
class ManagedObject;
class Assembly;
} // namespace dotnet

HYP_ENUM()
enum class ScriptComponentFlags : uint32
{
    NONE = 0x0,
    INITIALIZED = 0x1,
    RELOADING = 0x2,
    INITIALIZATION_STARTED = 0x4,
    ACTIVATED = 0x8,
    ERRORED = 0x10 //!< a lifecycle method threw an exception
};

HYP_MAKE_ENUM_FLAGS(ScriptComponentFlags);

// Saved value of one of a script's reflected ([HypField] / [HypProperty]) members.
// Held as HMF source via HMF::UnresolvedValue, so it's read with the member's own type, even after the script changes.
HYP_STRUCT(NoScriptBindings)
struct ScriptFieldValue
{
    HYP_STRUCT_BODY(ScriptFieldValue);

    HYP_FIELD(Property = "Name", Serialize)
    Name name;

    HYP_FIELD(Property = "Value", Serialize)
    BoxedValue value;
};

HYP_STRUCT(
    Component,
    NoScriptBindings,
    Label = "Script Component",
    Description = "A script component that can be attached to an entity.")
struct ScriptComponent
{
    HYP_STRUCT_BODY(ScriptComponent);

    HYP_FIELD(Property = "Script")
    Handle<ScriptAsset> script;

    // Applied whenever the script object is created
    HYP_FIELD(Property = "FieldValues", Serialize, Editor = false)
    Array<ScriptFieldValue> fieldValues;

    HYP_FIELD(NoScriptBindings, Transient)
    SharedPtr<dotnet::Assembly> assembly;

    HYP_FIELD(NoScriptBindings, Transient)
    ScriptObjectResource* scriptObjectResource = nullptr;

    HYP_FIELD(NoScriptBindings, Transient)
    Handle<ObjectBase> nativeObject;

    HYP_FIELD(Transient)
    EnumFlags<ScriptComponentFlags> flags = ScriptComponentFlags::NONE;
};

} // namespace Hyperion
