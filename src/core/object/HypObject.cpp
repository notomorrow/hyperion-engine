/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypObject.hpp>
#include <core/object/ObjectPool.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/containers/Stack.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>

namespace hyperion {

#pragma region HypObjectInitializerGuardBase

HypObjectInitializerGuardBase::HypObjectInitializerGuardBase(HypObjectPtr ptr)
    : ptr(ptr)
{
    AssertThrow(ptr.IsValid());

#ifdef HYP_DEBUG_MODE
    initializerThreadId = Threads::CurrentThreadId();
#else
    count = 0;
#endif

    // Push NONE to prevent our current flags from polluting allocations that happen in the constructor
    PushGlobalContext(HypObjectInitializerContext {
        .hypClass = ptr.GetClass(),
        .flags = HypObjectInitializerFlags::NONE });
}

HypObjectInitializerGuardBase::~HypObjectInitializerGuardBase()
{
    PopGlobalContext<HypObjectInitializerContext>();

    if (!ptr.IsValid())
    {
        return;
    }

    HypObjectInitializerContext* context = GetGlobalContext<HypObjectInitializerContext>();

    if ((!context || !(context->flags & HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION)) && !ptr.GetClass()->IsAbstract())
    {
        if (RC<dotnet::Class> managedClass = ptr.GetClass()->GetManagedClass())
        {
            ptr.GetObjectInitializer()->SetManagedObjectResource(AllocateResource<ManagedObjectResource>(ptr, managedClass));
        }
        else
        {
            HYP_LOG(Object, Warning,
                "HypObjectInitializerGuardBase: HypClass '{}' does not have a managed class associated with it. "
                "This means that the object will not be created in the managed runtime, and will not be accessible from C#.",
                ptr.GetClass()->GetName());
        }
    }

    FixupObjectInitializerPointer(ptr.GetPointer(), ptr.GetObjectInitializer());
}

#pragma endregion HypObjectInitializerGuardBase

#pragma region HypObjectBase

HypObjectBase::HypObjectBase()
    : m_initState(INIT_STATE_UNINITIALIZED)
{
    HypObjectInitializerContext* context = GetGlobalContext<HypObjectInitializerContext>();
    AssertThrow(context != nullptr);

    const SizeType size = context->hypClass->GetSize();
    AssertDebug(size > 0);

    const SizeType alignment = context->hypClass->GetAlignment();
    AssertDebug(alignment > 0);

    const SizeType headerOffset = ((sizeof(HypObjectHeader) + alignment - 1) / alignment) * alignment;

    m_header = reinterpret_cast<HypObjectHeader*>(uintptr_t(this) - headerOffset);
    m_header->IncRefWeak();
}

HypObjectBase::~HypObjectBase()
{
    AssertDebug(m_header != nullptr);
    m_header->DecRefWeak();
}

#pragma endregion HypObjectBase

#pragma region HypObjectPtr

HYP_API uint32 HypObjectPtr::GetRefCount_Strong() const
{
    if (!IsValid())
    {
        return 0;
    }

    IHypObjectInitializer* initializer = m_hypClass->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    return initializer->GetRefCount_Strong(m_ptr);
}

HYP_API uint32 HypObjectPtr::GetRefCount_Weak() const
{
    if (!IsValid())
    {
        return 0;
    }

    IHypObjectInitializer* initializer = m_hypClass->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    return initializer->GetRefCount_Weak(m_ptr);
}

HYP_API void HypObjectPtr::IncRef(bool weak)
{
    AssertDebug(IsValid());

    IHypObjectInitializer* initializer = m_hypClass->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    initializer->IncRef(m_ptr, weak);
}

HYP_API void HypObjectPtr::DecRef(bool weak)
{
    AssertDebug(IsValid());

    IHypObjectInitializer* initializer = m_hypClass->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    initializer->DecRef(m_ptr, weak);
}

HYP_API IHypObjectInitializer* HypObjectPtr::GetObjectInitializer() const
{
    if (!m_hypClass || !m_ptr)
    {
        return nullptr;
    }

    return m_hypClass->GetObjectInitializer(m_ptr);
}

HYP_API const HypClass* HypObjectPtr::GetHypClass(TypeId typeId) const
{
    const HypClass* hypClass = ::hyperion::GetClass(typeId);
    AssertThrow(hypClass != nullptr);

    if (m_ptr != nullptr)
    {
        const IHypObjectInitializer* initializer = hypClass->GetObjectInitializer(m_ptr);
        AssertThrow(initializer != nullptr);

        hypClass = initializer->GetClass();
        AssertThrow(hypClass != nullptr);
    }

    return hypClass;
}

#pragma endregion HypObjectPtr

#pragma region DynamicHypObjectInitializer

DynamicHypObjectInitializer::DynamicHypObjectInitializer(const HypClass* hypClass, IHypObjectInitializer* parentInitializer)
    : m_hypClass(hypClass),
      m_parentInitializer(parentInitializer)
{
    AssertThrow(hypClass != nullptr);
    AssertThrow(hypClass->IsDynamic());

    AssertThrowMsg(hypClass->IsReferenceCounted(),
        "DynamicHypObjectInitializer: HypClass is not reference counted. Class: %s"
        "\n\tDynamic HypClass types must be reference counted to ensure proper cleanup and deletion of allocations.",
        *hypClass->GetName());

    AssertThrow(parentInitializer != nullptr);

    AssertThrowMsg(parentInitializer->GetClass() == hypClass->GetParent(),
        "DynamicHypObjectInitializer: Parent initializer class does not match hyp_class parent class. Parent initializer class: %s, hyp_class parent class: %s",
        *parentInitializer->GetClass()->GetName(), *hypClass->GetParent()->GetName());
}

DynamicHypObjectInitializer::~DynamicHypObjectInitializer()
{
    // delete up the chain, if the parent is also dynamic.
    if (m_parentInitializer->GetClass()->IsDynamic())
    {
        delete m_parentInitializer;
    }
}

TypeId DynamicHypObjectInitializer::GetTypeId() const
{
    return m_hypClass->GetTypeId();
}

#pragma endregion DynamicHypObjectInitializer

HYP_API void FixupObjectInitializerPointer(void* target, IHypObjectInitializer* initializer)
{
    const HypClass* hypClass = initializer->GetClass();

    while (hypClass != nullptr)
    {
        hypClass->FixupPointer(target, initializer);

        const HypClass* parentHypClass = hypClass->GetParent();

        // Check to ensure no non-dynamic initializers are in the chain after the first dynamic one found.
        // This ensures we can properly delete the dynamic initializers up the chain, when the reference count goes to 0.
        if (parentHypClass && parentHypClass->IsDynamic())
        {
            AssertThrowMsg(hypClass->IsDynamic(),
                "Found a dynamic initializer in the chain, but the current initializer is not dynamic. Only dynamic initializers are allowed proceeding a dynamic initializer.\n\tCurrent initializer class: %s, next initializer class: %s",
                *hypClass->GetName(), *parentHypClass->GetName());
        }

        hypClass = parentHypClass;
    }
}

HYP_API void HypObject_OnIncRefCount_Strong(HypObjectPtr ptr, uint32 count)
{
    if (IHypObjectInitializer* initializer = ptr.GetObjectInitializer())
    {
        if (count > 1)
        {
            if (ManagedObjectResource* managedObjectResource = initializer->GetManagedObjectResource())
            {
                managedObjectResource->IncRef();
            }
        }
    }
}

HYP_API void HypObject_OnDecRefCount_Strong(HypObjectPtr ptr, uint32 count)
{
    if (IHypObjectInitializer* initializer = ptr.GetObjectInitializer())
    {
        if (count >= 1)
        {
            if (ManagedObjectResource* managedObjectResource = initializer->GetManagedObjectResource())
            {
                managedObjectResource->DecRef();
            }
        }
        else if (initializer->GetClass()->IsDynamic())
        {
            // Dynamic HypClass initializers need to be deleted manually, as they are not stored inline on the class and are heap allocated.
            // They will delete up the chain if the parent is also dynamic.
            // We don't allow any non-dynamic initializers in the chain after a dynamic one is added.
            delete initializer;
        }
    }
}

HYP_API void HypObject_OnIncRefCount_Weak(HypObjectPtr ptr, uint32 count)
{
}

HYP_API void HypObject_OnDecRefCount_Weak(HypObjectPtr ptr, uint32 count)
{
}

} // namespace hyperion