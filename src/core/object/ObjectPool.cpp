/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/ObjectPool.hpp>

namespace hyperion {

ObjectPool::ObjectContainerMap& ObjectPool::GetObjectContainerHolder()
{
    static ObjectPool::ObjectContainerMap holder {};

    return holder;
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

    return it->second.Get();
}

} // namespace hyperion