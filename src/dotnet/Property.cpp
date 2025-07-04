/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Property.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>

namespace hyperion::dotnet {

void Property::InvokeGetter_Internal(const Object* objectPtr, HypData* outReturnHypData)
{
    Assert(objectPtr != nullptr);
    Assert(objectPtr->GetClass() != nullptr);

    RC<Assembly> assembly = objectPtr->GetClass()->GetAssembly();

    assembly->GetInvokeGetterFunction()(m_guid, const_cast<ObjectReference*>(&objectPtr->GetObjectReference()), nullptr, outReturnHypData);
}

void Property::InvokeSetter_Internal(const Object* objectPtr, const HypData** valueHypData)
{
    Assert(objectPtr != nullptr);
    Assert(objectPtr->GetClass() != nullptr);

    RC<Assembly> assembly = objectPtr->GetClass()->GetAssembly();

    assembly->GetInvokeSetterFunction()(m_guid, const_cast<ObjectReference*>(&objectPtr->GetObjectReference()), valueHypData, nullptr);
}

} // namespace hyperion::dotnet