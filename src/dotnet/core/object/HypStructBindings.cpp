/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypStruct.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/Defines.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Object);

using DynamicHypStructInstance_DestructFunction = void (*)(void*);

class DynamicHypStructInstance final : public HypStruct
{
public:
    DynamicHypStructInstance(TypeId typeId, Name name, uint32 size, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members, DynamicHypStructInstance_DestructFunction destructFunction)
        : HypStruct(typeId, name, -1, 0, Name::Invalid(), attributes, flags, members),
          m_destructFunction(destructFunction)
    {
        m_size = size;
        m_alignment = alignof(void*);

        // @TODO Register the ManagedClass (dotnet::Class) for this. We need the assembly.
        HypClassRegistry::GetInstance().RegisterClass(typeId, this);
    }

    virtual ~DynamicHypStructInstance() override
    {
        HypClassRegistry::GetInstance().UnregisterClass(this);
    }

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override
    {
        Assert(objectPtr != nullptr);

        // Construct a new instance of the struct and return an ObjectReference pointing to it.
        if (!CreateStructInstance(outObjectReference, objectPtr, m_size))
        {
            return false;
        }

        return true;
    }
#endif

    virtual bool CanCreateInstance() const override
    {
        return true;
    }

    virtual bool ToHypData(ByteView memory, HypData& out) const override
    {
        void* data = Memory::Allocate(m_size);
        Memory::MemCpy(data, memory.Data(), m_size);

        out = HypData(Any::FromVoidPointer(m_typeId, data, m_destructFunction));

        return true;
    }

    virtual FBOMResult SerializeStruct(ConstAnyRef in, FBOMObject& out) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual FBOMResult DeserializeStruct(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
    }

    virtual bool CreateInstance_Internal(HypData& out) const override
    {
        HYP_NOT_IMPLEMENTED();

        return false;
    }

    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override
    {
        HYP_NOT_IMPLEMENTED();

        return false;
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    DynamicHypStructInstance_DestructFunction m_destructFunction;
};

extern "C"
{

    HYP_EXPORT HypStruct* HypStruct_CreateDynamicHypStruct(const TypeId* typeId, const char* typeName, uint32 size, DynamicHypStructInstance_DestructFunction destructFunction)
    {
        Assert(typeId != nullptr);
        Assert(typeName != nullptr);

        if (size == 0)
        {
            HYP_LOG(Object, Error, "Cannot create HypStruct with size 0");

            return nullptr;
        }

        return new DynamicHypStructInstance(*typeId, CreateNameFromDynamicString(typeName), size, Span<const HypClassAttribute>(), HypClassFlags::STRUCT_TYPE | HypClassFlags::DYNAMIC, Span<HypMember>(), destructFunction);
    }

    HYP_EXPORT void HypStruct_DestroyDynamicHypStruct(HypStruct* hypStruct)
    {
        Assert(hypStruct != nullptr);

        delete hypStruct;
    }

} // extern "C"

} // namespace hyperion