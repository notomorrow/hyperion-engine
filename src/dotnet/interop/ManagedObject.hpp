/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <dotnet/interop/ManagedGuid.hpp>

#include <core/Types.hpp>

namespace hyperion::dotnet {

extern "C"
{

    struct ObjectReference
    {
        void* weakHandle;
        void* strongHandle;

        bool operator==(const ObjectReference& other) const = default;
        bool operator!=(const ObjectReference& other) const = default;
    };

    static_assert(sizeof(ObjectReference) == 16, "ObjectReference size mismatch with C#");

} // extern "C"

} // namespace hyperion::dotnet
