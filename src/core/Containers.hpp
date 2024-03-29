#ifndef HYPERION_V2_CORE_CONTAINERS_H
#define HYPERION_V2_CORE_CONTAINERS_H

#include <core/lib/HashMap.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/StaticMap.hpp>
#include <core/lib/ArrayMap.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/HeapArray.hpp>
#include <core/lib/ByteBuffer.hpp>
#include <core/lib/StringView.hpp>
#include <core/lib/String.hpp>
#include <core/lib/Queue.hpp>
#include <core/lib/Stack.hpp>
#include <core/lib/Proc.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/Variant.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/Bitset.hpp>
#include <core/lib/Span.hpp>
#include <core/lib/LinkedList.hpp>
#include <core/lib/AtomicVar.hpp>
#include <math/MathUtil.hpp>
#include <Types.hpp>

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <tuple>
#include <mutex>

namespace hyperion::v2 {

class Engine;

template <class Group>
struct CallbackRef
{
    uint id;
    Group *group;
    typename Group::ArgsTuple bound_args;

    CallbackRef()
        : id(0),
          group(nullptr),
          bound_args{}
    {
    }

    CallbackRef(uint id, Group *group)
        : id(id),
          group(group),
          bound_args{}
    {
    }

    CallbackRef(const CallbackRef &other) = delete;
    CallbackRef &operator=(const CallbackRef &other) = delete;

    CallbackRef(CallbackRef &&other) noexcept
        : id(other.id),
          group(other.group),
          bound_args(std::move(other.bound_args))
    {
        other.id = 0;
        other.group = nullptr;
    }
    
    CallbackRef &operator=(CallbackRef &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        if (Valid()) {
            Remove();
        }

        id = other.id;
        group = other.group;
        bound_args = std::move(other.bound_args);

        other.id = 0;
        other.group = nullptr;

        return *this;
    }

    ~CallbackRef() = default;

    bool Valid() const { return id != 0 && group != nullptr; }

    bool Remove()
    {
        const bool result = Valid() && group->Remove(id);

        id = 0;
        group = nullptr;
        bound_args = {};

        return result;
    }

    CallbackRef &Bind(typename Group::ArgsTuple &&args)
    {
        bound_args = std::move(args);

        return *this;
    }

    bool Trigger()
    {
        if (!Valid()) {
            return false;
        }

        /* expand bound_args tuple into Trigger() function args */
        return std::apply([this]<class ...Args> (Args ... args) -> bool {
            return group->Trigger(id, std::forward<Args>(args)...);
        }, bound_args);
    }

    bool TriggerRemove()
    {
        if (!Valid()) {
            return false;
        }

        const auto id_before = id;

        id = 0; // invalidate

        /* expand bound_args tuple into Trigger() function args */
        const bool result = std::apply([&]<class ...Args> (Args ... args) -> bool {
            return group->Trigger(id_before, std::forward<Args>(args)...);
        }, bound_args);

        group->Remove(id_before);

        group = nullptr;
        bound_args = {};

        return result;
    }
};

// relly don't like this interface anymore.
// will refactor. the only reason it is still needed
// is so that stuff that uses certain descriptor sets gets
// created only after the descriptor sets are created.

template <class Enum, class ...Args>
class Callbacks
{
    struct CallbackInstance
    {
        using Function = std::function<void(Args...)>;

        using Id = uint;
        static constexpr Id empty_id = 0;

        Id id{empty_id};
        Function fn;
        uint num_calls{0};

        bool Valid() const        { return id != empty_id; }
        void Reset()              { id = empty_id; }

        uint NumCalls() const     { return num_calls; }
        
        template <class ...OtherArgs>
        void Call(OtherArgs &&... args)
        {
            AssertThrow(Valid());

            fn(std::forward<OtherArgs>(args)...);

            ++num_calls;
        }
    };

public:
    struct CallbackGroup {
        using ArgsTuple = std::tuple<Args...>;

        std::vector<CallbackInstance> once_callbacks;
        std::vector<CallbackInstance> on_callbacks;

        struct TriggerState {
            bool triggered{false};
            ArgsTuple args;
        } trigger_state = {};

        ~CallbackGroup()
        {
            std::lock_guard guard(m_mtx);

            for (auto &once_callback : once_callbacks) {
                once_callback.Reset();
            }

            for (auto &on_callback : on_callbacks) {
                on_callback.Reset();
            }
        }

        CallbackInstance *GetCallbackInstance(typename CallbackInstance::Id id)
        {
            std::lock_guard guard(m_mtx);

            auto once_it = Find(id, once_callbacks);

            if (once_it != once_callbacks.end()) {
                return &(*once_it);
            }

            auto on_it = Find(id, on_callbacks);

            if (on_it != on_callbacks.end()) {
                return &(*on_it);
            }

            return nullptr;
        }

        bool CheckValid(typename CallbackInstance::Id id)
        {
            if (CallbackInstance *instance = GetCallbackInstance(id)) {
                return instance->Valid();
            }

            return false;
        }

        bool Remove(typename CallbackInstance::Id id)
        {
            std::lock_guard guard(m_mtx);

            auto once_it = Find(id, once_callbacks);

            if (once_it != once_callbacks.end()) {
                once_it->Reset(); // invalidate; remove later

                return true;
            }

            auto on_it = Find(id, on_callbacks);

            if (on_it != on_callbacks.end()) {
                on_it->Reset(); // invalidate; remove later

                return true;
            }

            return false;
        }

        /*! \brief Trigger a specific callback, removing it if it is a `once` callback. */
        bool Trigger(uint id, Args &&... args)
        {
            std::lock_guard guard(m_mtx);

            auto once_it = Find(id, once_callbacks);

            if (once_it != once_callbacks.end()) {
                if (!once_it->Valid()) {
                    return false;
                }

                AssertThrowMsg(once_it->NumCalls() == 0, "'once' callback has already been called!");

                once_it->Call(std::forward<Args>(args)...);
                once_it->Reset();

                return true;
            }

            auto on_it = Find(id, on_callbacks);

            if (on_it != on_callbacks.end()) {
                if (!on_it->Valid()) {
                    return false;
                }

                on_it->Call(std::forward<Args>(args)...);

                return true;
            }

            return false;
        }

        void ClearInvalidatedCallbacks()
        {
            std::lock_guard guard(m_mtx);

            once_callbacks.erase(
                std::remove_if(once_callbacks.begin(), once_callbacks.end(), [](const auto &item) {
                    return !item.Valid();
                }),
                once_callbacks.end()
            );

            on_callbacks.erase(
                std::remove_if(on_callbacks.begin(), on_callbacks.end(), [](const auto &item) {
                    return !item.Valid();
                }),
                on_callbacks.end()
            );
        }

        void AddOnceCallback(CallbackInstance &&instance)
        {
            std::lock_guard guard(m_mtx);

            once_callbacks.push_back(std::move(instance));
        }

        void AddOnCallback(CallbackInstance &&instance)
        {
            std::lock_guard guard(m_mtx);

            on_callbacks.push_back(std::move(instance));
        }

    private:
        auto Find(typename CallbackInstance::Id id, std::vector<CallbackInstance> &callbacks) -> typename std::vector<CallbackInstance>::iterator
        {
            return std::find_if(
                callbacks.begin(),
                callbacks.end(),
                [id](const auto &other) {
                    return other.id == id;
                }
            );
        }

        std::recursive_mutex m_mtx; // eugh
    };

    using ArgsTuple = std::tuple<Args...>;

    Callbacks() = default;
    Callbacks(const Callbacks &other) = delete;
    Callbacks &operator=(const Callbacks &other) = delete;
    ~Callbacks() = default;

    CallbackRef<CallbackGroup>
    Once(Enum key, typename CallbackInstance::Function &&function)
    {
        std::lock_guard guard(rw_callbacks_mutex);

        auto &holder = m_holders[key];

        const auto id = ++m_id_counter;
        
        CallbackInstance callback_instance {
            .id = id,
            .fn = std::move(function)
        };

        if (holder.trigger_state.triggered) {
            callback_instance.Call(std::get<Args>(holder.trigger_state.args)...);

            return CallbackRef<CallbackGroup>();
        }

        holder.AddOnceCallback(std::move(callback_instance));

        return CallbackRef<CallbackGroup>(id, &holder);
    }

    CallbackRef<CallbackGroup>
    On(Enum key, typename CallbackInstance::Function &&function)
    {
        std::lock_guard guard(rw_callbacks_mutex);

        auto &holder = m_holders[key];

        const auto id = ++m_id_counter;
        
        CallbackInstance callback_instance{
            .id = id,
            .fn = std::move(function)
        };

        if (holder.trigger_state.triggered) {
            callback_instance.Call(std::get<Args>(holder.trigger_state.args)...);
        }

        holder.AddOnCallback(std::move(callback_instance));

        return CallbackRef<CallbackGroup>(id, &holder);
    }

    void Trigger(Enum key, Args &&... args)
    {
        TriggerCallbacks(false, key, std::move(args)...);
    }

    /* \brief Trigger all `once` and `on` events for the given key,
     * keeping the holder of all callbacks in that key in the triggered state,
     * so that any newly added callbacks will be executed immediately.
     */
    void TriggerPersisted(Enum key, Args &&... args)
    {
        TriggerCallbacks(true, key, std::move(args)...);
    }


private:
    uint m_id_counter { 0 };
    std::unordered_map<Enum, CallbackGroup> m_holders;

    std::mutex rw_callbacks_mutex;
    
    void TriggerCallbacks(bool persist, Enum key, Args &&... args)
    {
        std::lock_guard guard(rw_callbacks_mutex);

        auto &holder = m_holders[key];

        auto &once_callbacks = holder.once_callbacks;
        auto &on_callbacks = holder.on_callbacks;

        const bool previous_triggered_state = holder.trigger_state.triggered;

        holder.trigger_state.triggered = true;
        holder.trigger_state.args = std::tie(args...);

        for (CallbackInstance &callback_instance : once_callbacks) {
            if (callback_instance.Valid()) {
                callback_instance.Call(std::forward<Args>(args)...);
                callback_instance.Reset();
            }
        }

        for (CallbackInstance &callback_instance : on_callbacks) {
            if (callback_instance.Valid()) {
                callback_instance.Call(std::forward<Args>(args)...);
            }
        }

        holder.trigger_state.triggered = previous_triggered_state || persist;

        holder.ClearInvalidatedCallbacks();
    }
};

} // namespace hyperion::v2

#endif
