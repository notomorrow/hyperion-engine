/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/memory/resource/Resource.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/Types.hpp>

#ifndef HYP_DOTNET
#include <script/vm/Value.hpp>
#endif

namespace hyperion {

namespace dotnet {
class Class;
class Object;
class Method;
struct ObjectReference;
} // namespace dotnet

enum class ObjectFlags : uint32;

class HYP_API ManagedObjectResource final : public ResourceBase
{
public:
#ifdef HYP_DOTNET
    ManagedObjectResource(dotnet::Object* objectPtr, const RC<dotnet::Class>& managedClass);
    ManagedObjectResource(HypObjectPtr ptr, const RC<dotnet::Class>& managedClass);
    ManagedObjectResource(HypObjectPtr ptr, dotnet::Object* objectPtr, const RC<dotnet::Class>& managedClass);
    ManagedObjectResource(HypObjectPtr ptr, const RC<dotnet::Class>& managedClass, const dotnet::ObjectReference& objectReference, EnumFlags<ObjectFlags> objectFlags);
#else
    ManagedObjectResource(HypObjectPtr ptr);
#endif

    ManagedObjectResource(const ManagedObjectResource& other) = delete;
    ManagedObjectResource& operator=(const ManagedObjectResource& other) = delete;

    ManagedObjectResource(ManagedObjectResource&& other) noexcept;
    ManagedObjectResource& operator=(ManagedObjectResource&& other) noexcept = delete;

    ~ManagedObjectResource();

    HYP_FORCE_INLINE dotnet::Object* GetManagedObject() const
    {
        return m_objectPtr;
    }

    const RC<dotnet::Class> GetManagedClass() const
    {
        return m_managedClass;
    }

protected:
    virtual void Initialize() override final;
    virtual void Destroy() override final;

    HypObjectPtr m_ptr;

    dotnet::Object* m_objectPtr;
    RC<dotnet::Class> m_managedClass;
};

} // namespace hyperion
