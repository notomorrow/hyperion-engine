#ifndef HYPERION_V2_CORE_OBSERVER_H
#define HYPERION_V2_CORE_OBSERVER_H

#include <vector>

namespace hyperion::v2 {



template <class T, class Observer>
class Notifier {
public:
    using GetCurrentItemsFunction = std::function<std::vector<std::pair<T *, size_t>>()>;

    Notifier(GetCurrentItemsFunction &&get_current_items)
        : m_get_current_items(std::move(get_current_items)),
          m_id_counter(0)
    {
    }

    ~Notifier()
    {
        for (const auto &it : m_get_current_items()) {
            for (auto &observer : m_observers) {
                observer.m_on_items_removed(it.first, it.second);
                observer.m_ref.RemoveSoft();
            }
        }

        m_observers.clear();
    }

    typename Observer::Ref Add(Observer &&observer)
    {
        observer.m_ref = typename Observer::Ref(++m_id_counter, this);

        /* call with all current items. */
        for (const auto &it : m_get_current_items()) {
            observer.m_on_items_added(it.first, it.second);
        }

        m_observers.push_back(std::move(observer));

        return m_observers.back().m_ref;
    }

    void Remove(const typename Observer::Ref &ref)
    {
        auto it = std::find_if(
            m_observers.begin(),
            m_observers.end(),
            [&](const auto &item) {
                return item.m_ref.value == ref.value;
            }
        );

        if (it == m_observers.end()) {
            return;
        }
        
        for (const auto &items : m_get_current_items()) {
            it->m_on_items_removed(items.first, items.second);
        }

        it->m_ref.RemoveSoft();

        m_observers.erase(it);
    }

    void ItemAdded(T &item)
    {
        for (auto &observer : m_observers) {
            observer.m_on_items_added(&item, 1);
        }
    }

    void ItemRemoved(T &item)
    {
        for (auto &observer : m_observers) {
            observer.m_on_items_removed(&item, 1);
        }
    }

    void ItemsAdded(T *ptr, size_t count)
    {
        for (auto &observer : m_observers) {
            observer.m_on_items_added(ptr, count);
        }
    }

    void ItemsRemoved(T *ptr, size_t count)
    {
        for (auto &observer : m_observers) {
            observer.m_on_items_removed(ptr, count);
        }
    }

private:
    std::vector<Observer>   m_observers;
    GetCurrentItemsFunction m_get_current_items;
    uint32_t                m_id_counter;
};

template <class T>
class Observer {
public:
    using Function = std::function<void(T *ptr, size_t count)>;

    Observer(Function &&on_items_added, Function &&on_items_removed)
        : m_on_items_added(std::move(on_items_added)),
          m_on_items_removed(std::move(on_items_removed))
    {
    }

    Observer(const Observer &other) = delete;
    Observer &operator=(const Observer &other) = delete;

    Observer(Observer &&other) noexcept
        : m_on_items_added(std::move(other.m_on_items_added)),
          m_on_items_removed(std::move(other.m_on_items_removed))
    {
        other.m_on_items_added   = nullptr;
        other.m_on_items_removed = nullptr;
    }

    Observer &operator=(Observer &&other) noexcept
    {
        m_on_items_added   = std::move(other.m_on_items_added);
        m_on_items_removed = std::move(other.m_on_items_removed);
        other.m_on_items_added   = nullptr;
        other.m_on_items_removed = nullptr;

        return *this;
    }

    ~Observer()
    {
        Remove();
    }

    void Remove()
    {
        m_ref.Remove();
    }
    
    Function m_on_items_added,
             m_on_items_removed;
    
    
    struct Ref {
        struct Count {
            uint32_t count = 0;
        } *count_ptr;

        uint32_t               value    = 0;
        Notifier<T, Observer> *notifier = nullptr;

        Ref()
            : value(0),
              notifier(nullptr),
              count_ptr(new Count{1})
        {
        }

        Ref(uint32_t value, Notifier<T, Observer> *notifier)
            : value(value),
              notifier(notifier),
              count_ptr(new Count{1})
        {
        }

        Ref(const Ref &other)
            : value(other.value),
              notifier(other.notifier),
              count_ptr(other.count_ptr)
        {
            if (count_ptr != nullptr) {
                ++count_ptr->count;
            }
        }

        Ref &operator=(const Ref &other)
        {
            Remove();

            value     = other.value;
            notifier  = other.notifier;
            count_ptr = other.count_ptr;

            if (count_ptr != nullptr) {
                ++count_ptr->count;
            }

            return *this;
        }

        Ref(Ref &&other) noexcept
            : value(other.value),
              notifier(other.notifier),
              count_ptr(other.count_ptr)
        {
            other.value     = 0;
            other.notifier  = nullptr;
            other.count_ptr = nullptr;
        }

        Ref &operator=(Ref &&other) noexcept
        {
            Remove();

            value     = other.value;
            notifier  = other.notifier;
            count_ptr = other.count_ptr;

            other.value     = 0;
            other.notifier  = nullptr;
            other.count_ptr = nullptr;

            return *this;
        }


        ~Ref()
        {
            Remove();
        }

        operator bool() const { return value != 0 && notifier != nullptr && count_ptr != nullptr; }

        void RemoveSoft()
        {
            if (count_ptr != nullptr) {
                --count_ptr->count;

                if (count_ptr->count == 0) {
                    delete count_ptr;
                }

                count_ptr = nullptr;
            }

            value    = 0;
            notifier = nullptr;
        }

        void Remove()
        {
            if (notifier != nullptr) {
                notifier->Remove(*this);
            }

            RemoveSoft();
        }
    } m_ref;
};

template <class T>
using ObserverNotifier = Notifier<T, Observer<T>>;

} // namespace hyperion::v2

#endif