/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <Types.hpp>

namespace hyperion::dotnet {

extern "C"
{

    HYP_EXPORT DelegateHandler* ScriptableDelegate_Bind(IScriptableDelegate* delegate, dotnet::Class* classObjectPtr, ObjectReference* objectReference)
    {
        AssertThrow(delegate != nullptr);
        AssertThrow(objectReference != nullptr);
        AssertThrow(classObjectPtr != nullptr);

        return new DelegateHandler(delegate->BindManaged("DynamicInvoke", MakeUnique<dotnet::Object>(classObjectPtr->RefCountedPtrFromThis(), *objectReference, ObjectFlags::CREATED_FROM_MANAGED)));
    }

    HYP_EXPORT int ScriptableDelegate_RemoveAllDetached(IScriptableDelegate* delegate)
    {
        AssertThrow(delegate != nullptr);

        return delegate->RemoveAllDetached();
    }

    HYP_EXPORT int8 ScriptableDelegate_Remove(IScriptableDelegate* delegate, DelegateHandler* delegateHandler)
    {
        AssertThrow(delegate != nullptr);

        if (!delegateHandler)
        {
            return 0;
        }

        return delegate->Remove(std::move(*delegateHandler));
    }

    HYP_EXPORT void DelegateHandler_Detach(DelegateHandler* delegateHandler)
    {
        AssertThrow(delegateHandler != nullptr);

        delegateHandler->Detach();
    }

    HYP_EXPORT void DelegateHandler_Remove(DelegateHandler* delegateHandler)
    {
        AssertThrow(delegateHandler != nullptr);

        delegateHandler->Reset();
    }

    HYP_EXPORT void DelegateHandler_Destroy(DelegateHandler* delegateHandler)
    {
        AssertThrow(delegateHandler != nullptr);

        delete delegateHandler;
    }

} // extern "C"

} // namespace hyperion::dotnet