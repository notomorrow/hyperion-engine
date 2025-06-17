/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Helpers.hpp>
#include <dotnet/Object.hpp>

#include <core/filesystem/FilePath.hpp>

namespace hyperion::dotnet {

// void *TransformArgument<Object *>::operator()(Object *value) const
// {
//     return value->GetObjectReference().weakHandle;
// }

const char* TransformArgument<FilePath>::operator()(const FilePath& value) const
{
    return value.Data();
}

} // namespace hyperion::dotnet