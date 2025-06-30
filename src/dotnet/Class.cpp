/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/Class.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion::dotnet {

Class::~Class() = default;

RC<Assembly> Class::GetAssembly() const
{
    RC<Assembly> assembly = m_assembly.Lock();

    if (!assembly || !assembly->IsLoaded())
    {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }

    return assembly;
}

Object* Class::NewObject()
{
    AssertThrowMsg(m_newObjectFptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ObjectReference objectReference = m_newObjectFptr(/* keepAlive */ true, nullptr, nullptr, nullptr, nullptr);

    return new Object(RefCountedPtrFromThis(), objectReference);
}

Object* Class::NewObject(const HypClass* hypClass, void* owningObjectPtr)
{
    AssertThrow(hypClass != nullptr);
    AssertThrow(owningObjectPtr != nullptr);

    AssertThrowMsg(m_newObjectFptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    ObjectReference objectReference = m_newObjectFptr(/* keepAlive */ true, hypClass, owningObjectPtr, nullptr, nullptr);

    return new Object(RefCountedPtrFromThis(), objectReference);
}

ObjectReference Class::NewManagedObject(void* contextPtr, InitializeObjectCallbackFunction callback)
{
    AssertThrowMsg(m_newObjectFptr != nullptr, "New object function pointer not set for managed class %s", m_name.Data());

    return m_newObjectFptr(/* keepAlive */ false, nullptr, nullptr, contextPtr, callback);
}

bool Class::HasParentClass(UTF8StringView parentClassName) const
{
    const Class* parentClass = m_parentClass;

    while (parentClass)
    {
        if (parentClass->GetName() == parentClassName)
        {
            return true;
        }

        parentClass = parentClass->GetParentClass();
    }

    return false;
}

bool Class::HasParentClass(const Class* parentClass) const
{
    const Class* currentParentClass = m_parentClass;

    while (currentParentClass)
    {
        if (currentParentClass == parentClass)
        {
            return true;
        }

        currentParentClass = currentParentClass->GetParentClass();
    }

    return false;
}

void Class::InvokeStaticMethod_Internal(const Method* methodPtr, const HypData** argsHypData, HypData* outReturnHypData)
{
    RC<Assembly> assembly = m_assembly.Lock();

    if (!assembly || !assembly->IsLoaded())
    {
        HYP_THROW("Cannot use managed class: assembly has been unloaded");
    }

    methodPtr->Invoke({}, argsHypData, outReturnHypData);
}

} // namespace hyperion::dotnet