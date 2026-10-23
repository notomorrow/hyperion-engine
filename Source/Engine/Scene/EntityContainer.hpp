/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Threading/DataRaceDetector.hpp>

#include <Core/Reflection/ObjId.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Framework/EngineMemory.hpp>

#include <Scene/ComponentContainer.hpp>

namespace Hyperion {

CORE_API extern size_t GetNumDescendants(TypeId typeId);
CORE_API extern int GetSubclassIndex(TypeId baseTypeId, const Class* subclass);

class Class;
class Entity;

using ComponentMap = Map<TypeId, ComponentId, SceneAllocator>;

struct EntityData
{
    // Keep a weak handle around so the Entity pointer doesn't get invalidated and reused
    WeakHandle<Entity> entityWeak;
    ComponentMap components;

    template <class Component>
    HYP_FORCE_INLINE bool HasComponent() const
    {
        return components.Contains(TypeId::ForType<Component>());
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
        auto* kvp = components.TryGet(TypeId::ForType<Component>());
        return kvp ? kvp->second : Invalid<ComponentId>;
    }

    HYP_FORCE_INLINE ComponentId GetComponentId(TypeId componentTypeId) const
    {
        auto* kvp = components.TryGet(componentTypeId);
        return kvp ? kvp->second : Invalid<ComponentId>;
    }

    template <class Component>
    HYP_FORCE_INLINE Optional<ComponentId> TryGetComponentId() const
    {
        auto it = components.Find(TypeId::ForType<Component>());

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
    HYP_FORCE_INLINE ComponentMap::Iterator FindComponent()
    {
        return components.Find(TypeId::ForType<Component>());
    }

    HYP_FORCE_INLINE ComponentMap::Iterator FindComponent(TypeId componentTypeId)
    {
        return components.Find(componentTypeId);
    }

    template <class Component>
    HYP_FORCE_INLINE ComponentMap::ConstIterator FindComponent() const
    {
        return components.Find(TypeId::ForType<Component>());
    }

    HYP_FORCE_INLINE ComponentMap::ConstIterator FindComponent(TypeId componentTypeId) const
    {
        return components.Find(componentTypeId);
    }
};

class EntityContainer
{
    struct SubtypeData
    {
        SparsePagedArray<EntityData, 256, SceneAllocator> data;
    };

public:
    ENGINE_API static EntityContainer& GetDefaultInstance();

    EntityContainer()
    {
        // +1 for Entity itself
        m_subtypeData.Resize(GetNumDescendants(TypeId::ForType<Entity>()) + 1);
    }

    HYP_FORCE_INLINE Span<SubtypeData> GetSubtypeData()
    {
        return m_subtypeData.ToSpan();
    }

    HYP_FORCE_INLINE Span<const SubtypeData> GetSubtypeData() const
    {
        return m_subtypeData.ToSpan();
    }

    void Add(const Handle<Entity>& entity)
    {
        AssertDebug(entity != nullptr);

        const ObjId<Entity> id = entity.Id();

        SubtypeData& subtypeData = GetSubtypeData(entity.ptr);
        AssertDebug(!subtypeData.data.HasIndex(id.ToIndex()), "Entity with ID {} already exists in EntityContainer!", id);

        subtypeData.data.Emplace(id.ToIndex(), EntityData { entity.ToWeak() });
    }

    bool Remove(const ObjectBase* entity)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!entity)
        {
            return false;
        }

        SubtypeData& subtypeData = GetSubtypeData(entity);
        const uint32 index = entity->Id().ToIndex();

        if (!subtypeData.data.HasIndex(index))
        {
            return false;
        }

        subtypeData.data.EraseAt(index);

        return true;
    }

    bool HasEntity(const ObjectBase* entity) const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return entity != nullptr && GetSubtypeData(entity).data.HasIndex(entity->Id().ToIndex());
    }

    EntityData* TryGetEntityData(const ObjectBase* entity)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!entity)
        {
            return nullptr;
        }

        return GetSubtypeData(entity).data.TryGet(entity->Id().ToIndex());
    }

    const EntityData* TryGetEntityData(const ObjectBase* entity) const
    {
        return const_cast<EntityContainer*>(this)->TryGetEntityData(entity);
    }

    HYP_FORCE_INLINE EntityData& GetEntityData(const ObjectBase* entity)
    {
        EntityData* data = TryGetEntityData(entity);
        AssertDebug(data != nullptr);

        return *data;
    }

    HYP_FORCE_INLINE const EntityData& GetEntityData(const ObjectBase* entity) const
    {
        const EntityData* data = TryGetEntityData(entity);
        AssertDebug(data != nullptr);

        return *data;
    }

private:
    SubtypeData& GetSubtypeData(const ObjectBase* entity)
    {
        const int classIndex = GetSubclassIndex(TypeId::ForType<Entity>(), entity->InstanceClass()) + 1;
        AssertDebug(classIndex >= 0, "Invalid class index {}", classIndex);
        AssertDebug(classIndex < m_subtypeData.Size(), "Invalid class index {}", classIndex);

        return m_subtypeData[classIndex];
    }

    const SubtypeData& GetSubtypeData(const ObjectBase* entity) const
    {
        return const_cast<EntityContainer*>(this)->GetSubtypeData(entity);
    }

    Array<SubtypeData, SceneAllocator> m_subtypeData;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace Hyperion
