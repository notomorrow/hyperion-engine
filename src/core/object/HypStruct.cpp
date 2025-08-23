/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypStruct.hpp>

#ifdef HYP_DOTNET
#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#endif

namespace hyperion {

bool HypStruct::CreateStructInstance(dotnet::ObjectReference& outObjectReference, const void* objectPtr, SizeType size) const
{
    struct ManagedStructInitializerContext
    {
        const void* ptr;
        SizeType size;
    };

    HYP_CORE_ASSERT(objectPtr != nullptr);
    
#ifdef HYP_DOTNET
    if (dotnet::Class* managedClass = HypClass::GetManagedClass())
    {
        ManagedStructInitializerContext context;
        context.ptr = objectPtr;
        context.size = size;

        outObjectReference = managedClass->NewManagedObject(&context, [](void* contextPtr, void* objectPtr, uint32 objectSize)
            {
                ManagedStructInitializerContext& context = *static_cast<ManagedStructInitializerContext*>(contextPtr);

                HYP_CORE_ASSERT(objectSize == context.size, "Type size does not match managed struct size! Expected managed struct to have size of %zu but got %u",
                    context.size, objectSize);

                Memory::MemCpy(objectPtr, context.ptr, context.size);
            });

        return true;
    }
#endif

    return false;
}

} // namespace hyperion