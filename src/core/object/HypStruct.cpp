/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypStruct.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>

namespace hyperion {

bool HypStruct::CreateStructInstance(dotnet::ObjectReference& outObjectReference, const void* objectPtr, SizeType size) const
{
    struct ManagedStructInitializerContext
    {
        const void* ptr;
        SizeType size;
    };

    AssertThrow(objectPtr != nullptr);

    if (dotnet::Class* managedClass = HypClass::GetManagedClass())
    {
        ManagedStructInitializerContext context;
        context.ptr = objectPtr;
        context.size = size;

        outObjectReference = managedClass->NewManagedObject(&context, [](void* contextPtr, void* objectPtr, uint32 objectSize)
            {
                ManagedStructInitializerContext& context = *static_cast<ManagedStructInitializerContext*>(contextPtr);

                AssertThrowMsg(objectSize == context.size, "Type size does not match managed struct size! Expected managed struct to have size of %llu but got %u",
                    context.size, objectSize);

                Memory::MemCpy(objectPtr, context.ptr, context.size);
            });

        return true;
    }

    return false;
}

} // namespace hyperion