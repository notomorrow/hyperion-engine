/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <core/object/ObjectPool.hpp>
#include <core/object/HypClass.hpp>

namespace hyperion {

HYP_API void ReleaseHypClassInstance(const HypClass* hypClass, uint32 index)
{
    HYP_CORE_ASSERT(hypClass != nullptr, "HypClass is null");
    HYP_CORE_ASSERT(index != ~0u, "Invalid index");

    ObjectContainerBase* container = hypClass->GetObjectContainer();
    HYP_CORE_ASSERT(container != nullptr, "HypClass has no ObjectContainer");

    hypClass->GetObjectContainer()->ReleaseIndex(index);
}

static ObjectPool::ObjectContainerMap g_objectContainerMap {};

ObjectPool::ObjectContainerMap::~ObjectContainerMap()
{
    for (auto& it : m_map)
    {
        if (it.second != nullptr)
        {
            delete it.second;
            it.second = nullptr;
        }
    }
}

ObjectPool::ObjectContainerMap& ObjectPool::GetObjectContainerMap()
{
    return g_objectContainerMap;
}

ObjectContainerBase& ObjectPool::ObjectContainerMap::GetOrCreate(TypeId typeId, ObjectContainerBase* (*createFn)(void))
{
    Mutex::Guard guard(m_mutex);

    auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it != m_map.End())
    {
        if (it->second == nullptr)
        {
            it->second = createFn();

            HYP_CORE_ASSERT(it->second != nullptr);
        }

        return *it->second;
    }

    ObjectContainerBase* container = createFn();
    HYP_CORE_ASSERT(container != nullptr);

    return *m_map.EmplaceBack(typeId, container).second;
}

ObjectContainerBase& ObjectPool::ObjectContainerMap::Get(TypeId typeId)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it == m_map.End())
    {
        HYP_FAIL("No object container for TypeId: %u", typeId.Value());
    }

    HYP_CORE_ASSERT(it->second != nullptr);

    return *it->second;
}

ObjectContainerBase* ObjectPool::ObjectContainerMap::TryGet(TypeId typeId)
{
    Mutex::Guard guard(m_mutex);

    const auto it = m_map.FindIf([typeId](const auto& element)
        {
            return element.first == typeId;
        });

    if (it == m_map.End())
    {
        return nullptr;
    }

    return it->second;
}

} // namespace hyperion