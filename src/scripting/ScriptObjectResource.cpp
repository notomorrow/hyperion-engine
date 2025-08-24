#include <scripting/ScriptObjectResource.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/logging/Logger.hpp>

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Resource);
HYP_DECLARE_LOG_CHANNEL(Object);

ScriptObjectResource::ScriptObjectResource(dotnet::Object* objectPtr, const RC<dotnet::Class>& managedClass)
    : m_objectPtr(objectPtr),
      m_managedClass(managedClass)
{
}

ScriptObjectResource::ScriptObjectResource(HypObjectPtr ptr, const RC<dotnet::Class>& managedClass)
    : ScriptObjectResource(ptr, managedClass, {}, ObjectFlags::NONE)
{
}

ScriptObjectResource::ScriptObjectResource(HypObjectPtr ptr, dotnet::Object* objectPtr, const RC<dotnet::Class>& managedClass)
    : m_ptr(ptr),
      m_objectPtr(objectPtr),
      m_managedClass(managedClass)
{
}

ScriptObjectResource::ScriptObjectResource(HypObjectPtr ptr, const RC<dotnet::Class>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags)
    : m_ptr(ptr),
      m_objectPtr(nullptr),
      m_managedClass(managedClass)
{
    if (m_ptr && m_managedClass)
    {
        void* address = m_ptr.GetPointer();

        if (objectFlags & ObjectFlags::CREATED_FROM_MANAGED)
        {
            m_objectPtr = new dotnet::Object(m_managedClass->RefCountedPtrFromThis(), objectReference, ObjectFlags::CREATED_FROM_MANAGED);
        }
        else
        {
            /// VVVV NOTE: moved HypObject_IncRef into C# constructor instead
            // if (m_ptr.GetClass()->IsReferenceCounted())
            //{
            //     // Increment reference count for the managed object (creating from managed does this already via HypObject_Initialize())
            //     // The managed object is responsible for decrementing the ref count using HypObject_DecRef() in finalizer and Dispose().
            //     m_ptr.IncRef();
            // }

            HYP_LOG(Object, Debug, "Creating new managed object with class {}, reference will be incremented from C#", m_managedClass->GetName());

            m_objectPtr = m_managedClass->NewObject(m_ptr.GetClass(), address);
        }

        HYP_CORE_ASSERT(m_objectPtr != nullptr);
    }
}

#ifdef HYP_SCRIPT

ScriptObjectResource::ScriptObjectResource(HypObjectPtr ptr, vm::Value value)
    : m_ptr(ptr),
      m_value(value)
{
}

#endif

ScriptObjectResource::~ScriptObjectResource()
{
    if (m_objectPtr)
    {
        delete m_objectPtr;
        m_objectPtr = nullptr;
    }
}

ScriptLanguage ScriptObjectResource::GetScriptLanguage() const
{
#ifdef HYP_SCRIPT
    if (m_value.IsValid())
    {
        return SL_SCRIPT;
    }
#endif

#ifdef HYP_DOTNET
    if (m_objectPtr)
    {
        return SL_CSHARP;
    }
#endif

    return SL_INVALID;
}

void ScriptObjectResource::Initialize()
{
    if (!m_objectPtr)
    {
        return;
    }

    if (m_objectPtr->SetKeepAlive(true))
    {
        return;
    }

    if (!m_ptr.IsValid())
    {
        HYP_LOG(Object, Error, "Thread: {}\tManaged object could not be kept alive, it may have been garbage collected\n\tObject address: {}",
            Threads::CurrentThreadId().GetName(),
            (void*)m_objectPtr);

        return;
    }

    // Need to recreate the managed object; could be queued for finalization.
    // In this case, the ref count will be decremented once the queued object is finalized
    const HypClass* hypClass = m_ptr.GetClass();

    HYP_LOG(Object, Info, "Thread: {}\tManaged object for object with HypClass {} at address {} could not be kept alive, it may have been garbage collected. The managed object will be recreated.\n\tObject address: {}",
        Threads::CurrentThreadId().GetName(),
        hypClass->GetName(), m_ptr.GetPointer(),
        (void*)m_objectPtr);

    if (m_managedClass)
    {
        //// VVVV NOTE: moved HypObject_IncRef into C# constructor instead
        /*if (hypClass->IsReferenceCounted())
        {
            m_ptr.IncRef();
        }*/

        dotnet::Object* newManagedObject = m_managedClass->NewObject(hypClass, m_ptr.GetPointer());

        if (!newManagedObject)
        {
            HYP_FAIL("Failed to recreate managed object for HypClass %s", hypClass->GetName().LookupString());
        }

        delete m_objectPtr;

        m_objectPtr = newManagedObject;
    }
    else
    {
        HYP_FAIL("Failed to recreate managed object for HypClass %s", hypClass->GetName().LookupString());
    }
}

void ScriptObjectResource::Destroy()
{
    if (m_objectPtr)
    {
        const bool result = m_objectPtr->SetKeepAlive(false);

        HYP_CORE_ASSERT(result);
    }
}

} // namespace hyperion