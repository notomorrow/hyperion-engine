/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <DotNET/Interop/ManagedGuid.hpp>

#include <Core/Defines.hpp>

#include <type_traits>

namespace Hyperion {

struct BoxedValue;

namespace dotnet {

struct ObjectReference;

using Delegate = std::add_pointer_t<void()>;

///Returns false if the managed method threw an exception
using InvokeMethodFunction = bool (*)(void* context, ObjectReference*, const BoxedValue**, BoxedValue*);

struct ManagedInvoker
{
    InvokeMethodFunction fptr = nullptr;
    void* context = nullptr;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return fptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator()(ObjectReference* thisObjectReference, const BoxedValue** argsBoxed, BoxedValue* outBoxed) const
    {
        return fptr != nullptr && fptr(context, thisObjectReference, argsBoxed, outBoxed);
    }
};

} // namespace dotnet
} // namespace Hyperion
