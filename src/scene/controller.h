#ifndef HYPERION_V2_CONTROLLER_H
#define HYPERION_V2_CONTROLLER_H

#include <rendering/base.h>
#include "../game_counter.h"

#include <util/sparse_map/sparse_map.h>

#include <atomic>
#include <unordered_map>
#include <type_traits>

namespace hyperion::v2 {

using ControllerId = uint32_t;

extern std::atomic<ControllerId> controller_id_counter;

class Node;
class Engine;

class Controller {
    friend class Node;

public:
    Controller(const char *name);
    Controller(const Controller &other) = delete;
    Controller &operator=(const Controller &other) = delete;
    virtual ~Controller();

    const char *GetName() const { return m_name; }
    Node *GetParent() const     { return m_parent; }

protected:
    virtual void OnAdded() = 0;
    virtual void OnRemoved() = 0;
    virtual void OnUpdate(GameCounter::TickUnit delta) = 0;

    virtual void OnDescendentAdded(Node *descendent) {}
    virtual void OnDescendentRemoved(Node *descendent) {}

private:
    char *m_name;
    Node *m_parent;
};

template <class T>
ControllerId GetControllerId()
{
    static_assert(std::is_base_of_v<Controller, T>, "Object must be a derived class of Controller");

    static ControllerId id = ++controller_id_counter;

    return id;
}

class ControllerSet {
    using Map = tsl::sparse_map<ControllerId, std::unique_ptr<Controller>>;

public:
    ControllerSet() = default;
    ControllerSet(const ControllerSet &other) = delete;
    ControllerSet &operator=(const ControllerSet &other) = delete;

    ControllerSet(ControllerSet &&other) noexcept
        : m_map(std::move(other.m_map))
    {
    }

    ControllerSet &operator=(ControllerSet &&other) noexcept
    {
        m_map = std::move(other.m_map);

        return *this;
    }

    ~ControllerSet() = default;
    
    template <class T>
    void Set(std::unique_ptr<T> &&controller)
    {
        if (controller == nullptr) {
            return;
        }

        const auto id = GetControllerId<T>();

        m_map[id] = std::move(controller);
    }

    template <class T>
    T *Get()
    {
        const auto id = GetControllerId<T>();

        const auto it = m_map.find(id);

        if (it == m_map.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    template <class T>
    bool Has() const
    {
        const auto id = GetControllerId<T>();

        return m_map.find(id) != m_map.end();
    }
    
    template <class T>
    bool Remove()
    {
        const auto id = GetControllerId<T>();

        const auto it = m_map.find(id);

        if (it == m_map.end()) {
            return false;
        }

        m_map.erase(it);

        return true;
    }

    Map::iterator begin() { return m_map.begin(); }
    Map::iterator end()   { return m_map.end(); }
    Map::const_iterator cbegin() const { return m_map.cbegin(); }
    Map::const_iterator cend() const   { return m_map.cend(); }

private:
    Map m_map;
};

} // namespace hyperion::v2

#endif