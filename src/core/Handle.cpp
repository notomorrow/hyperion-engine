/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Handle.hpp>
#include <core/object/HypClass.hpp>

namespace hyperion {

#pragma region AnyHandle

const AnyHandle AnyHandle::empty = {};

AnyHandle::AnyHandle(HypObjectBase* hypObjectPtr)
    : ptr(hypObjectPtr),
      typeId(hypObjectPtr ? hypObjectPtr->m_header->container->GetObjectTypeId() : TypeId::Void())
{
    if (IsValid())
    {
        ptr->m_header->IncRefStrong();
    }
}

AnyHandle::AnyHandle(const HypClass* hypClass, HypObjectBase* ptr)
    : ptr(ptr),
      typeId(hypClass ? hypClass->GetTypeId() : TypeId::Void())
{
    if (IsValid())
    {
        ptr->m_header->IncRefStrong();
    }
}

AnyHandle::AnyHandle(const AnyHandle& other)
    : typeId(other.typeId),
      ptr(other.ptr)
{
    if (IsValid())
    {
        ptr->m_header->IncRefStrong();
    }
}

AnyHandle& AnyHandle::operator=(const AnyHandle& other)
{
    if (this == &other)
    {
        return *this;
    }

    const bool wasSamePtr = ptr == other.ptr;

    if (!wasSamePtr)
    {
        if (IsValid())
        {
            ptr->m_header->DecRefStrong();
        }
    }

    ptr = other.ptr;
    typeId = other.typeId;

    if (!wasSamePtr)
    {
        if (IsValid())
        {
            ptr->m_header->IncRefStrong();
        }
    }

    return *this;
}

AnyHandle::AnyHandle(AnyHandle&& other) noexcept
    : typeId(other.typeId),
      ptr(other.ptr)
{
    other.ptr = nullptr;
}

AnyHandle& AnyHandle::operator=(AnyHandle&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (IsValid())
    {
        ptr->m_header->DecRefStrong();
    }

    ptr = other.ptr;
    typeId = other.typeId;

    other.ptr = nullptr;

    return *this;
}

AnyHandle::~AnyHandle()
{
    if (IsValid())
    {
        ptr->m_header->DecRefStrong();
    }
}

AnyHandle::IdType AnyHandle::Id() const
{
    if (!IsValid())
    {
        return IdType();
    }

    return IdType { ptr->m_header->container->GetObjectTypeId(), ptr->m_header->index + 1 };
}

TypeId AnyHandle::GetTypeId() const
{
    return typeId;
}

AnyRef AnyHandle::ToRef() const
{
    if (!IsValid())
    {
        return AnyRef(typeId, nullptr);
    }

    return AnyRef(typeId, ptr);
}

void AnyHandle::Reset()
{
    if (IsValid())
    {
        ptr->m_header->DecRefStrong();
    }

    ptr = nullptr;
}

void* AnyHandle::Release()
{
    if (!IsValid())
    {
        return nullptr;
    }

    void* address = ptr;
    ptr = nullptr;

    return address;
}

#pragma endregion AnyHandle

} // namespace hyperion