/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatMap.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Spinlock.hpp>

#include <core/object/ObjId.hpp>
#include <core/Handle.hpp>

#include <scene/ComponentContainer.hpp>

namespace hyperion {

class Entity;

struct EntityData
{
    // Keep a weak handle around so the Entity pointer doesn't get invalidated and reused
    WeakHandle<Entity> entityWeak;
    TypeMap<ComponentId> components;

    template <class Component>
    HYP_FORCE_INLINE bool HasComponent() const
    {
        return components.Contains<Component>();
    }

    HYP_FORCE_INLINE bool HasComponent(TypeId componentTypeId) const
    {
        return components.Contains(componentTypeId);
    }

    template <class... Components>
    HYP_FORCE_INLINE bool HasComponents() const
    {
        return (HasComponent<Components>() && ...);
    }

    HYP_FORCE_INLINE bool HasComponents(Span<const TypeId> componentTypeIds) const
    {
        for (const TypeId& typeId : componentTypeIds)
        {
            if (!components.Contains(typeId))
            {
                return false;
            }
        }

        return true;
    }

    template <class Component>
    HYP_FORCE_INLINE ComponentId GetComponentId() const
    {
        return components.At<Component>();
    }

    HYP_FORCE_INLINE ComponentId GetComponentId(TypeId componentTypeId) const
    {
        return components.At(componentTypeId);
    }

    template <class Component>
    HYP_FORCE_INLINE Optional<ComponentId> TryGetComponentId() const
    {
        auto it = components.Find<Component>();

        if (it == components.End())
        {
            return {};
        }

        return it->second;
    }

    HYP_FORCE_INLINE Optional<ComponentId> TryGetComponentId(TypeId componentTypeId) const
    {
        auto it = components.Find(componentTypeId);

        if (it == components.End())
        {
            return {};
        }

        return it->second;
    }

    template <class Component>
    HYP_FORCE_INLINE typename TypeMap<ComponentId>::Iterator FindComponent()
    {
        return components.Find<Component>();
    }

    HYP_FORCE_INLINE typename TypeMap<ComponentId>::Iterator FindComponent(TypeId componentTypeId)
    {
        return components.Find(componentTypeId);
    }

    template <class Component>
    HYP_FORCE_INLINE typename TypeMap<ComponentId>::ConstIterator FindComponent() const
    {
        return components.Find<Component>();
    }

    HYP_FORCE_INLINE typename TypeMap<ComponentId>::ConstIterator FindComponent(TypeId componentTypeId) const
    {
        return components.Find(componentTypeId);
    }
};

class EntityContainer
{
public:
    using Iterator = HashMap<Entity*, EntityData>::Iterator;
    using ConstIterator = HashMap<Entity*, EntityData>::ConstIterator;

    HYP_FORCE_INLINE void Add(Entity* entity)
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        AssertDebug(entity != nullptr);
        {
            auto it = m_entities.Find(entity);
            AssertDebug(it == m_entities.End(), "Entity with ID {} already exists in EntityContainer! Found: {}", entity->Id(), it->first->Id());
        }

        m_entities[entity] = { entity->WeakHandleFromThis() };
    }

    HYP_FORCE_INLINE bool Remove(const Entity* entity)
    {
        if (!entity)
        {
            return false;
        }

        HYP_MT_CHECK_RW(m_dataRaceDetector);

        auto it = m_entities.FindAs(entity);
        if (it == m_entities.End())
        {
            return false;
        }

        m_entities.Erase(it);

        return true;
    }

    HYP_FORCE_INLINE bool HasEntity(const Entity* entity) const
    {
        if (!entity)
        {
            return false;
        }

        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_entities.FindAs(entity) != m_entities.End();
    }

    HYP_FORCE_INLINE EntityData* TryGetEntityData(const Entity* entity)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        auto it = m_entities.FindAs(entity);

        return it != m_entities.End() ? &it->second : nullptr;
    }

    HYP_FORCE_INLINE const EntityData* TryGetEntityData(const Entity* entity) const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        auto it = m_entities.FindAs(entity);

        return it != m_entities.End() ? &it->second : nullptr;
    }

    HYP_FORCE_INLINE EntityData& GetEntityData(const Entity* entity)
    {
        EntityData* data = TryGetEntityData(entity);
        Assert(data != nullptr);

        return *data;
    }

    HYP_FORCE_INLINE const EntityData& GetEntityData(const Entity* entity) const
    {
        const EntityData* data = TryGetEntityData(entity);
        Assert(data != nullptr);

        return *data;
    }

    HYP_DEF_STL_BEGIN_END(m_entities.Begin(), m_entities.End())

private:
    HashMap<Entity*, EntityData> m_entities;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace hyperion
