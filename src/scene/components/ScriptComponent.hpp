/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/EnumFlags.hpp>

#include <scripting/Script.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

namespace dotnet {
class Object;
class Assembly;
} // namespace dotnet

class ManagedObjectResource;
class ScriptAsset;

enum class ScriptComponentFlags : uint32
{
    NONE = 0x0,
    INITIALIZED = 0x1,
    RELOADING = 0x2,
    INITIALIZATION_STARTED = 0x4,
    BEFORE_INIT_CALLED = 0x10,
    INIT_CALLED = 0x20 // the script has already been compiled once, with Init() and BeforeInit() called. don't call them again.
};

HYP_MAKE_ENUM_FLAGS(ScriptComponentFlags);

HYP_STRUCT(Component, Label = "Script Component", Description = "A script component that can be attached to an entity.")
struct ScriptComponent
{
    HYP_FIELD()
    Handle<ScriptAsset> scriptAsset;

    HYP_FIELD()
    RC<dotnet::Assembly> assembly;

    HYP_FIELD()
    ManagedObjectResource* managedObjectResource = nullptr;

    HYP_FIELD()
    EnumFlags<ScriptComponentFlags> flags = ScriptComponentFlags::NONE;
};

} // namespace hyperion
