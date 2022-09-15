#ifndef HYPERION_V2_LIB_COMPONENT_SET_H
#define HYPERION_V2_LIB_COMPONENT_SET_H

#include <core/lib/UniquePtr.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

template <class Component>
class ComponentSetUnique
{
public:
    using ComponentID = UInt;
    using Map = FlatMap<ComponentID, std::unique_ptr<Component>>;

private:
    static inline std::atomic<ComponentID> id_counter;

public:
    template <class T>
    static ComponentID GetComponentID()
    {
        static_assert(std::is_base_of_v<Component, T>, "Object must be a derived class of the component class");

        static const ComponentID id = ++id_counter;

        return id;
    }

    using Iterator = typename Map::Iterator;
    using ConstIterator = typename Map::ConstIterator;

    ComponentSetUnique() = default;
    ComponentSetUnique(const ComponentSetUnique &other) = delete;
    ComponentSetUnique &operator=(const ComponentSetUnique &other) = delete;

    ComponentSetUnique(ComponentSetUnique &&other) noexcept
        : m_map(std::move(other.m_map))
    {
    }

    ComponentSetUnique &operator=(ComponentSetUnique &&other) noexcept
    {
        m_map = std::move(other.m_map);

        return *this;
    }

    ~ComponentSetUnique()
    {
        Clear();
    }
    
    template <class T>
    void Set(std::unique_ptr<T> &&component)
    {
        AssertThrow(component != nullptr);

        const auto id = GetComponentID<T>();

        m_map[id] = std::move(component);
    }

    /*! Only use if you know what you're doing (That the object has the exact type id) */
    void Set(ComponentID id, std::unique_ptr<Component> &&component)
    {
        AssertThrow(component != nullptr);

        m_map[id] = std::move(component);
    }

    template <class T>
    T *Get() const
    {
        const auto id = GetComponentID<T>();

        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return nullptr;
        }

        return static_cast<T *>(it->second.get());
    }

    Component *Get(ComponentID id) const
    {
        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return nullptr;
        }

        return it->second.get();
    }

    template <class T>
    bool Has() const
    {
        const auto id = GetComponentID<T>();

        return m_map.Find(id) != m_map.End();
    }
    
    bool Has(ComponentID id) const
    {
        return m_map.Find(id) != m_map.End();
    }
    
    template <class T>
    bool Remove()
    {
        const auto id = GetComponentID<T>();

        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return false;
        }

        if (it->second != nullptr) {
            it->second->ComponentRemoved();
        }

        m_map.Erase(it);

        return true;
    }
    
    bool Remove(ComponentID id)
    {
        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return false;
        }

        if (it->second != nullptr) {
            it->second->ComponentRemoved();
        }

        m_map.Erase(it);

        return true;
    }

    void Clear()
    {
        for (auto it = m_map.Begin(); it != m_map.End(); ++it) {
            if (it->second == nullptr) {
                continue;
            }

            it->second->ComponentRemoved();
        }

        m_map.Clear();
    }

    HYP_DEF_STL_BEGIN_END(m_map.begin(), m_map.end());

private:
    Map m_map;
};

} // namespace hyperion::v2

#endif