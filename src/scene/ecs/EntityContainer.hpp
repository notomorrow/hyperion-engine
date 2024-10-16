/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_CONTAINER_HPP
#define HYPERION_ECS_ENTITY_CONTAINER_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/threading/DataRaceDetector.hpp>
#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <scene/ecs/ComponentContainer.hpp>

namespace hyperion {

class Entity;

struct EntityData
{
    Handle<Entity>          handle;
    TypeMap<ComponentID>    components;

    template <class Component>
    HYP_FORCE_INLINE bool HasComponent() const
    {
        return components.Contains<Component>();
    }

    HYP_FORCE_INLINE bool HasComponent(TypeID component_type_id) const
    {
        return components.Contains(component_type_id);
    }

    template <class ... Components>
    HYP_FORCE_INLINE bool HasComponents() const
    {
        return (HasComponent<Components>() && ...);
    }

    template <class Component>
    HYP_FORCE_INLINE ComponentID GetComponentID() const
    {
        return components.At<Component>();
    }

    HYP_FORCE_INLINE ComponentID GetComponentID(TypeID component_type_id) const
    {
        return components.At(component_type_id);
    }
};

class EntityContainer
{
public:
    using Iterator          = FlatMap<ID<Entity>, EntityData>::Iterator;
    using ConstIterator     = FlatMap<ID<Entity>, EntityData>::ConstIterator;

    using KeyValuePairType  = FlatMap<ID<Entity>, EntityData>::KeyValuePairType;

    HYP_FORCE_INLINE ID<Entity> AddEntity(Handle<Entity> handle)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        const ID<Entity> id = handle.GetID();

        EntityData data;
        data.handle = std::move(handle);

        const auto insert_result = m_entities.Insert(id, std::move(data));

        return insert_result.first->first;
    }

    HYP_FORCE_INLINE ID<Entity> AddEntity(ID<Entity> id, EntityData data)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        const auto insert_result = m_entities.Insert(id, std::move(data));

        return insert_result.first->first;
    }

    HYP_FORCE_INLINE EntityData &GetEntityData(ID<Entity> id)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.At(id);
    }

    HYP_FORCE_INLINE const EntityData &GetEntityData(ID<Entity> id) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.At(id);
    }

    HYP_FORCE_INLINE Iterator Find(ID<Entity> id)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.Find(id);
    }

    HYP_FORCE_INLINE ConstIterator Find(ID<Entity> id) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.Find(id);
    }

    HYP_FORCE_INLINE void Erase(Iterator it)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        m_entities.Erase(it);
    }

    HYP_FORCE_INLINE void Erase(ConstIterator it)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        m_entities.Erase(it);
    }

    HYP_DEF_STL_BEGIN_END(
        m_entities.Begin(),
        m_entities.End()
    )

private:
    FlatMap<ID<Entity>, EntityData> m_entities;
    DataRaceDetector                m_data_race_detector;
};

} // namespace hyperion

#endif