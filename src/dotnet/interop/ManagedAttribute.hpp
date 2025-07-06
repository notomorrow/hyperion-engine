/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/UniquePtr.hpp>

#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <Types.hpp>

namespace hyperion::dotnet {

class Class;

extern "C"
{

    struct ManagedAttribute
    {
        Class* classPtr;
        ObjectReference objectReference;
    };

    static_assert(sizeof(ManagedAttribute) == 24, "sizeof(ManagedAttribute) must match C# struct size");

    struct ManagedAttributeHolder
    {
        uint32 managedAttributesSize;
        ManagedAttribute* managedAttributesPtr;
    };

    static_assert(sizeof(ManagedAttributeHolder) == 16, "sizeof(ManagedAttributeHolder) must match C# struct size");

} // extern "C"

} // namespace hyperion::dotnet
