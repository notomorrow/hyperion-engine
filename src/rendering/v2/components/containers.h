#ifndef HYPERION_V2_CONTAINERS_H
#define HYPERION_V2_CONTAINERS_H

#include <math/math_util.h>
#include <util/range.h>

#include <vector>
#include <memory>
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <functional>
#include <tuple>

namespace hyperion::v2 {

class Engine;

template <class T>
struct ObjectIdHolder {
    std::vector<typename T::ID> ids;

    inline constexpr size_t Size() const
        { return ids.size(); }
    
    void Add(typename T::ID id)
    {
        ids.push_back(id);
    }

    void Remove(typename T::ID id)
    {
        const auto it = std::find(ids.begin(), ids.end(), id);

        if (it != ids.end()) {
            ids.erase(it);
        }
    }

    bool Has(typename T::ID id) const
    {
        return std::any_of(ids.begin(), ids.end(), [id](auto &it) { return it == id; });
    }
};

template <class Group>
struct CallbackRef {
    using RemoveFunction = std::function<void()>;

    uint32_t id;
    Group *group;
    typename Group::ArgsTuple bound_args;

    CallbackRef()
        : id(0),
          group(nullptr),
          bound_args{}
    {
    }

    CallbackRef(uint32_t id, Group *group)
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
        return std::apply([this]<class ...Args>(Args ... args) -> bool {
            return group->Trigger(id, std::forward<Args>(args)...);
        }, bound_args);
    }
};

template <class Enum, class ...Args>
class Callbacks {
    struct CallbackInstance {
        using Function = std::function<void(Args...)>;

        uint32_t id;
        Function fn;

        bool Valid() const { return id != 0; }
    };

public:
    struct CallbackGroup {
        using ArgsTuple = std::tuple<Args...>;

        std::vector<CallbackInstance> once_callbacks;
        std::vector<CallbackInstance> on_callbacks;

        struct TriggerState {
            bool triggered = false;
            ArgsTuple args;
        } trigger_state = {};

        auto Find(uint32_t id, std::vector<CallbackInstance> &callbacks) -> typename std::vector<CallbackInstance>::iterator
        {
            return std::find_if(
                callbacks.begin(),
                callbacks.end(),
                [id](auto &other) { return other.id == id; }
            );
        }

        bool Remove(uint32_t id)
        {
            auto once_it = Find(id, once_callbacks);

            if (once_it != once_callbacks.end()) {
                once_callbacks.erase(once_it);

                return true;
            }

            auto on_it = Find(id, on_callbacks);

            if (on_it != on_callbacks.end()) {
                on_callbacks.erase(on_it);

                return true;
            }

            return false;
        }

        /*! \brief Trigger a specific callback, removing it if it is a `once` callback. */
        bool Trigger(uint32_t id, Args &&... args)
        {
            auto once_it = Find(id, once_callbacks);

            if (once_it != once_callbacks.end()) {
                once_it->fn(std::forward<Args>(args)...);
                once_callbacks.erase(once_it);

                return true;
            }

            auto on_it = Find(id, on_callbacks);

            if (on_it != on_callbacks.end()) {
                on_it->fn(std::forward<Args>(args)...);

                return true;
            }

            return false;
        }
    };

    using ArgsTuple = std::tuple<Args...>;

    Callbacks() = default;
    Callbacks(const Callbacks &other) = delete;
    Callbacks &operator=(const Callbacks &other) = delete;
    ~Callbacks() = default;

    auto Once(Enum key, typename CallbackInstance::Function &&function)
    {
        auto &holder = m_holders[key];

        const auto id = ++m_id_counter;
        
        CallbackInstance callback_instance{
            .id = id,
            .fn = std::move(function)
        };

        if (holder.trigger_state.triggered) {
            callback_instance.fn(std::get<Args>(holder.trigger_state.args)...);

            return CallbackRef<CallbackGroup>();
        }

        holder.once_callbacks.push_back(std::move(callback_instance));

        return CallbackRef<CallbackGroup>(id, &holder);
    }

    auto On(Enum key, typename CallbackInstance::Function &&function)
    {
        auto &holder = m_holders[key];

        const auto id = ++m_id_counter;
        
        CallbackInstance callback_instance{
            .id = id,
            .fn = std::move(function)
        };

        if (holder.trigger_state.triggered) {
            callback_instance.fn(std::get<Args>(holder.trigger_state.args)...);
        }

        holder.on_callbacks.push_back(std::move(callback_instance));

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

    /*! \brief Trigger a specific callback (by the given ID).
     * The callback can either be a `once` or `on` callback.
     * @returns A boolean stating whether the callback was executed or not
     */
    bool TriggerSpecific(Enum key, uint32_t id, Args &&... args)
    {
        auto &holder = m_holders[key];

        return holder.Trigger(id, std::forward<Args>(args)...);
    }

private:
    uint32_t m_id_counter = 0;
    std::unordered_map<Enum, CallbackGroup> m_holders;
    
    void TriggerCallbacks(bool persist, Enum key, Args &&... args)
    {
        auto &holder = m_holders[key];

        /* make copies so that callbacks can call Remove() without invalidation... */
        auto once_callbacks = holder.once_callbacks;
        auto on_callbacks   = holder.on_callbacks;

        const bool previous_triggered_state = holder.trigger_state.triggered;

        holder.trigger_state.triggered = true;
        holder.trigger_state.args = std::tie(args...);

        for (CallbackInstance &callback_instance : once_callbacks) {
            callback_instance.fn(std::forward<Args>(args)...);
        }

        holder.once_callbacks.clear();

        for (CallbackInstance &callback_instance : on_callbacks) {
            callback_instance.fn(std::forward<Args>(args)...);
        }
        
        holder.trigger_state.triggered = previous_triggered_state || persist;
    }
};

template <class CallbacksClass>
class CallbackTrackable {
public:
    using Callbacks = CallbacksClass;
    using CallbackRef = CallbackRef<typename Callbacks::CallbackGroup>;

    CallbackTrackable() = default;
    CallbackTrackable(const CallbackTrackable &other) = delete;
    CallbackTrackable &operator=(const CallbackTrackable &other) = delete;
    ~CallbackTrackable() = default;

protected:
    struct CallbackTracker {
        std::vector<CallbackRef> refs;
    };

    /*! \brief Triggers the destroy callback (if present) and
     * removes all existing callbacks from the callback holder
     */
    void Teardown()
    {
        if (m_init_callback.Valid()) {
            m_init_callback.Remove();
        }

        if (m_destroy_callback.Valid()) {
            m_destroy_callback.Trigger();
            m_destroy_callback.Remove();
        }
    }

    /*! \brief Add the given callback reference to this object's internal
     * list of callback refs it is responsible for. On destructor call, all remaining
     * callback refs will be removed, so as to not have a dangling callback.
     * @param callback_ref The callback reference, obtained via Once() or On()
     */
    void OnInit(CallbackRef &&callback_ref)
    {
        if (m_init_callback.Valid()) {
            DebugLog(LogType::Warn, "OnInit callback overwritten!\n");

            AssertThrow(m_init_callback.Remove());
        }

        m_init_callback = std::move(callback_ref);
    }

    /*! \brief Set the action to be triggered on teardown
     * @param callback_ref The callback reference, obtained via Once() or On()
     * @param bind_args Arguments to bind to the call
     */
    template <class ...Args>
    void OnTeardown(CallbackRef &&callback_ref, Args &&... bind_args)
    {
        if (m_destroy_callback.Valid()) {
            DebugLog(LogType::Warn, "OnTeardown callback overwritten!\n");

            AssertThrow(m_destroy_callback.Remove());
        }

        callback_ref.Bind(std::tie(std::forward<Args>(bind_args)...));

        m_destroy_callback = std::move(callback_ref);
    }

private:
    CallbackRef m_init_callback;
    CallbackRef m_destroy_callback;
};

enum class EngineCallback {
    NONE,

    CREATE_SCENES,
    DESTROY_SCENES,

    CREATE_SPATIALS,
    DESTROY_SPATIALS,

    CREATE_MESHES,
    DESTROY_MESHES,

    CREATE_TEXTURES,
    DESTROY_TEXTURES,

    CREATE_MATERIALS,
    DESTROY_MATERIALS,

    CREATE_SKELETONS,
    DESTROY_SKELETONS,

    CREATE_SHADERS,
    DESTROY_SHADERS,

    CREATE_RENDER_PASSES,
    DESTROY_RENDER_PASSES,

    CREATE_FRAMEBUFFERS,
    DESTROY_FRAMEBUFFERS,

    CREATE_GRAPHICS_PIPELINES,
    DESTROY_GRAPHICS_PIPELINES,

    CREATE_COMPUTE_PIPELINES,
    DESTROY_COMPUTE_PIPELINES
};

using EngineCallbacks = Callbacks<EngineCallback, Engine *>;

/* v1 callback utility still used by octree */
template <class CallbacksClass>
struct ComponentEvents {
    struct CallbackGroup {
        using CallbackFunction = typename CallbacksClass::CallbackFunction;

        std::vector<CallbackFunction> callbacks;

        template <class ...Args>
        void operator()(Args &&... args)
        {
            for (auto &callback : callbacks) {
                callback(args...);
            }
        }

        CallbackGroup &operator+=(const CallbackFunction &callback)
        {
            callbacks.push_back(callback);

            return *this;
        }

        CallbackGroup &operator+=(CallbackFunction &&callback)
        {
            callbacks.push_back(std::move(callback));

            return *this;
        }

        inline void Clear() { callbacks.clear(); }
    };

    CallbackGroup on_init,
                  on_deinit,
                  on_update;
};



/* Map from ObjectType::ID to another resource */
template <class ObjectType, class ValueType>
class ObjectMap {
public:
    ObjectMap() = default;
    ObjectMap(const ObjectMap &other) = delete;
    ObjectMap &operator=(const ObjectMap &other) = delete;
    ObjectMap(ObjectMap &&other) noexcept
        : m_map(std::move(other.m_map))
    {}
    ~ObjectMap() = default;

    ObjectMap &operator=(ObjectMap &&other) noexcept
    {
        m_map = std::move(other.m_map);

        return *this;
    }

    bool Has(typename ObjectType::ID id) const
    {
        return m_map.find(id.value) != m_map.end();
    }

    ValueType &Get(typename ObjectType::ID id)
        { return m_map.at(id.value); }

    const ValueType &Get(typename ObjectType::ID id) const
        { return m_map.at(id.value); }

    void Set(typename ObjectType::ID id, ValueType &&value)
    {
        m_map[id.value] = value;
    }

    void Remove(typename ObjectType::ID id)
    {
        m_map.erase(id.value);
    }

    void Clear()
    {
        m_map.clear();
    }

    size_t Size() const
    {
        return m_map.size();
    }

    inline ValueType &operator[](typename ObjectType::ID id)
        { return m_map[id.value]; }

    auto begin() { return m_map.begin(); }
    auto end() { return m_map.end(); }

private:
    std::unordered_map<typename ObjectType::ID::ValueType, ValueType> m_map;
};

template <class T>
struct ObjectHolder {
    bool defer_create = false;
    std::vector<std::unique_ptr<T>> objects;
    
    std::queue<size_t> free_slots;

    constexpr size_t Size() const
        { return objects.size(); }

    constexpr T *Get(typename T::ID id)
    {
        return MathUtil::InRange(id.value, {1, objects.size() + 1})
            ? objects[id.value - 1].get()
            : nullptr;
    }

    constexpr const T *Get(const typename T::ID &id) const
    {
        return const_cast<ObjectHolder<T> *>(this)->Get(id);
    }

    constexpr T *operator[](typename T::ID id) { return Get(id); }
    constexpr const T *operator[](typename T::ID id) const { return Get(id); }

    template <class LambdaFunction>
    constexpr typename T::ID Find(LambdaFunction lambda) const
    {
        const auto it = std::find_if(objects.begin(), objects.end(), lambda);

        if (it != objects.end()) {
            return typename T::ID{typename T::ID::ValueType(it - objects.begin() + 1)};
        }

        return T::bad_id;
    }
    
    T *Allot(std::unique_ptr<T> &&object)
    {
        if (!free_slots.empty()) {
            const auto next_id = typename T::ID{typename T::ID::ValueType(free_slots.front() + 1)};

            object->SetId(next_id);

            objects[free_slots.front()] = std::move(object);

            free_slots.pop();

            return objects[next_id.value - 1].get();
        }

        const auto next_id = typename T::ID{typename T::ID::ValueType(objects.size() + 1)};

        object->SetId(next_id);

        objects.push_back(std::move(object));

        return objects[next_id.value - 1].get();
    }
    
    template <class ...Args>
    auto Add(Engine *engine, std::unique_ptr<T> &&object, Args &&... args) -> typename T::ID
    {
        if (!free_slots.empty()) {
            const auto next_id = typename T::ID{typename T::ID::ValueType(free_slots.front() + 1)};

            object->SetId(next_id);

            if (!defer_create) {
                object->Create(engine, std::move(args)...);
            }

            objects[free_slots.front()] = std::move(object);

            free_slots.pop();

            return next_id;
        }

        const auto next_id = typename T::ID{typename T::ID::ValueType(objects.size() + 1)};

        object->SetId(next_id);

        if (!defer_create) {
            object->Create(engine, std::move(args)...);
        }

        objects.push_back(std::move(object));
            
        return next_id;
    }

    template<class ...Args>
    inline void Remove(Engine *engine, typename T::ID id, Args &&... args)
    {
        if (T *object = Get(id)) {
            object->Destroy(engine, std::move(args)...);

            /* Cannot simply erase from vector, as that would invalidate existing IDs */
            objects[id.value - 1].reset();
            
            free_slots.push(id.value - 1);
        }
    }

    template<class ...Args>
    void RemoveAll(Engine *engine, Args &&...args)
    {
        for (auto &object : objects) {
            if (object == nullptr) {
                continue;
            }

            object->Destroy(engine, std::move(args)...);
            object.reset();
        }
    }

    template <class ...Args>
    void CreateAll(Engine *engine, Args &&... args)
    {
        AssertThrowMsg(defer_create, "Expected defer_create to be true, "
            "otherwise objects are automatically have Create() called when added.");

        for (auto &object : objects) {
            object->Create(engine, std::move(args)...);
        }
    }
};

/* New ObjectHolder class that does not call Destroy,
 * as new component types will not have Destroy() rather using
 * a teardown method which is defined in CallbackTrackable.
 */
template <class T, class CallbacksClass>
struct ObjectVector {
    std::vector<std::unique_ptr<T>> objects;
    std::queue<size_t> free_slots;
    CallbacksClass &callbacks;

    ObjectVector(CallbacksClass &callbacks)
        : callbacks(callbacks)
    {
    }

    ObjectVector(const ObjectVector &other) = delete;
    ObjectVector &operator=(const ObjectVector &other) = delete;
    ~ObjectVector() {}

    constexpr size_t Size() const
        { return objects.size(); }

    auto &Objects() { return objects; }
    const auto &Objects() const { return objects; }

    constexpr T *Get(typename T::ID id)
    {
        return MathUtil::InRange(id.value, {1, objects.size() + 1})
            ? objects[id.value - 1].get()
            : nullptr;
    }

    constexpr const T *Get(typename T::ID id) const
        { return const_cast<ObjectVector<T, CallbacksClass> *>(this)->Get(id); }

    constexpr T *operator[](typename T::ID id)             { return Get(id); }
    constexpr const T *operator[](typename T::ID id) const { return Get(id); }

    template <class LambdaFunction>
    constexpr const T *Find(LambdaFunction lambda) const
    {
        const auto it = std::find_if(objects.begin(), objects.end(), lambda);

        if (it != objects.end()) {
            return *it;
        }

        return nullptr;
    }
    
    T *Add(std::unique_ptr<T> &&object)
    {
        AssertThrow(object != nullptr);
        AssertThrowMsg(object->GetId() == T::bad_id, "Adding object that already has id set");

        typename T::ID next_id;

        if (!free_slots.empty()) {
            next_id = typename T::ID{typename T::ID::ValueType(free_slots.front() + 1)};
            object->SetId(next_id);

            objects[free_slots.front()] = std::move(object);
            free_slots.pop();
        } else {
            next_id = typename T::ID{typename T::ID::ValueType(objects.size() + 1)};
            object->SetId(next_id);

            objects.push_back(std::move(object));
        }

        return objects[next_id.value - 1].get();
    }
    
    void Remove(typename T::ID id)
    {
        if (id.value == objects.size()) {
            objects.pop_back();

            return;
        }
        
        /* Cannot simply erase from vector, as that would invalidate existing IDs */
        objects[id.value - 1].reset();
        
        free_slots.push(id.value - 1);
    }
    
    void RemoveAll() { objects.clear(); }
};

template <class T, class CallbacksClass>
class RefCounter {
    using ArgsTuple = typename CallbacksClass::ArgsTuple;

    struct RefCount {
        uint32_t count = 0;
    };

    ObjectVector<T, CallbacksClass>   m_holder;
    mutable ObjectMap<T, RefCount>    m_ref_map;

    ArgsTuple                         m_init_args{};

public:
    class RefWrapper {
    public:
        RefWrapper()
            : RefWrapper(nullptr)
        {
        }

        RefWrapper(std::nullptr_t)
            : ptr(nullptr),
              m_ref_counter(nullptr)
        {
        }

        RefWrapper(T *ptr, RefCounter *ref_counter)
            : ptr(ptr),
              m_ref_counter(ref_counter)
        {
        }

        RefWrapper(const RefWrapper &other) = delete;
        RefWrapper &operator=(const RefWrapper &other) = delete;

        RefWrapper(RefWrapper &&other) noexcept
            : ptr(other.ptr),
              m_ref_counter(other.m_ref_counter)
        {
            other.ptr = nullptr;
            other.m_ref_counter = nullptr;
        }

        RefWrapper &operator=(RefWrapper &&other)
        {
            if (std::addressof(other) == this) {
                return *this;
            }

            std::swap(ptr, other.ptr);
            std::swap(m_ref_counter, other.m_ref_counter);

            if (other.Valid() && !operator==(other)) {
                other.Release();
            }

            return *this;
        }

        ~RefWrapper()
        {
            if (ptr != nullptr) {
                Release();
            }
        }

        operator T const * () const { return ptr; }
        operator bool() const       { return Valid(); }

        T *operator->()             { return ptr; }
        const T *operator->() const { return ptr; }

        T &operator*()              { return *ptr; }
        const T &operator*() const  { return *ptr; }
        
        bool operator==(std::nullptr_t) const
            { return ptr == nullptr; }

        bool operator!=(std::nullptr_t) const
            { return ptr != nullptr; }

        bool operator==(const RefWrapper &other) const
            { return &other == this || (ptr == other.ptr && m_ref_counter == other.m_ref_counter); }

        bool operator!=(const RefWrapper &other) const
            { return !operator==(other); }
        
        /*! \brief _If_ ptr has not had Init() performed yet (checked via IsInit()), call it, using
         * the pre-bound arguments from the reference counter this wrapper was acquired from.
         * Throws an assertion error if state is invalid.
         */
        void Init()
        {
            AssertState();
            
            if (!ptr->IsInit()) {
                /* Call ptr->Init() with the ref counter's pre-bound args */
                std::apply(&T::Init, std::tuple_cat(std::make_tuple(ptr), m_ref_counter->m_init_args));
            }
        }

        /*! \brief _If_ ptr has not had Init() performed yet (checked via IsInit()), call it.
         * Throws an assertion error if state is invalid.
         * @param args The arguments to be forwarded to Init()
         */
        template <class ...Args>
        void Init(Args &&... args)
        {
            AssertState();
            
            if (!ptr->IsInit()) {
                /* Call ptr->Init() with the provided args */
                ptr->Init(std::forward<Args>(args)...);
            }
        }

        /*! \brief Acquire a new reference from this one. Increments the reference counter. */
        auto Acquire()
        {
            AssertState();

            return m_ref_counter->Acquire(ptr);
        }

        size_t GetRefCount() const
        {
            if (!Valid()) {
                return 0;
            }

            return m_ref_counter->GetRefCount(ptr->GetId());
        }

        T *ptr;

    private:
        bool Valid() const
            { return ptr != nullptr && m_ref_counter != nullptr; }

        void AssertState()
        {
            AssertThrowMsg(ptr != nullptr,           "invalid state -- underlying pointer was null");
            AssertThrowMsg(m_ref_counter != nullptr, "invalid state -- ref counter not set");
        }

        void Release()
        {
            AssertState();

            m_ref_counter->Release(ptr);

            ptr = nullptr;
            m_ref_counter = nullptr;
        }

        RefCounter *m_ref_counter;
    };

    RefCounter(CallbacksClass &callbacks)
        : m_holder(callbacks),
          m_ref_map{}
    {
    }

    RefCounter(CallbacksClass &callbacks, ArgsTuple &&bind_init_args)
        : m_holder(callbacks),
          m_ref_map{},
          m_init_args(std::move(bind_init_args))
    {
    }

    ~RefCounter()
    {
        for (auto &it : m_ref_map) {
            auto &rc = it.second;

            if (rc.count == 0) { /* not yet initialized */
                DebugLog(
                    LogType::Warn,
                    "Ref to object of type %s was never initialized\n",
                    typeid(T).name()
                );
            } else {
                --rc.count;
            }

            AssertThrowMsg(rc.count == 0, "Destructor called while object still in use elsewhere");
        }
    }

    /*! \brief Sets the args tuple that is passed to Init() for any newly acquired object. */
    void BindInitArguments(ArgsTuple &&args)
    {
        m_init_args = std::move(args);
    }
    
    [[nodiscard]] RefWrapper Add(std::unique_ptr<T> &&object)
    {
        if (object == nullptr) {
            return nullptr;
        }

        T *ptr = m_holder.Add(std::move(object));
        m_ref_map[ptr->GetId()] = {.count = 1};
        
        return RefWrapper(ptr, this);
    }
    
    [[nodiscard]] RefWrapper Acquire(T *ptr)
    {
        AssertThrow(ptr != nullptr);

        ++m_ref_map[ptr->GetId()].count;
        
        return RefWrapper(ptr, this);
    }

    [[nodiscard]] RefWrapper Get(typename T::ID id)
    {
        T *ptr = m_holder.Get(id);

        if (ptr == nullptr) {
            return nullptr;
        }

        ++m_ref_map[ptr->GetId()].count;

        return RefWrapper(ptr, this);
    }

    [[nodiscard]] RefWrapper Get(typename T::ID id) const
        { return const_cast<const RefCounter *>(this)->Get(id); }
    
    void Release(const T *ptr)
    {
        AssertThrow(ptr != nullptr);

        const auto id = ptr->GetId();
        
        AssertThrowMsg(m_ref_map.Has(id), "Refcount not set");

        auto &counter = m_ref_map[id];
        AssertThrowMsg(counter.count != 0, "Cannot decrement refcount when already at zero");
        
        if (!--counter.count) {
            m_holder.Remove(id);
            m_ref_map.Remove(id);
        }
    }

    void Release(typename T::ID id)
    {
        Release(Get(id));
    }

    constexpr T *operator[](const typename T::ID &id) { return Get(id); }
    constexpr const T *operator[](const typename T::ID &id) const { return Get(id); }

    size_t GetRefCount(const typename T::ID &id) const
    {
        if (!m_ref_map.Has(id)) {
            return 0;
        }

        return m_ref_map.Get(id).count;
    }

    auto &Objects() { return m_holder.objects; }
    const auto &Objects() const { return m_holder.objects; }
};

template <class T>
using Ref = typename RefCounter<T, EngineCallbacks>::RefWrapper;

} // namespace hyperion::v2

#endif