#ifndef HYPERION_V2_COMPONENTS_BASE_H
#define HYPERION_V2_COMPONENTS_BASE_H

#include <core/Core.hpp>
#include <core/Containers.hpp>
#include <Component.hpp>
#include <core/Class.hpp>
#include <core/Handle.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/StaticString.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <HashCode.hpp>
#include <util/Defines.hpp>
#include <Constants.hpp>
#include <Types.hpp>

#include <memory>
#include <atomic>
#include <type_traits>


namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;

template <class Parent>
struct AttachmentSet
{
    using Initializer = std::add_pointer_t<void(WeakHandleBase &, Parent *)>;

    Initializer init_fn;
    Array<WeakHandleBase> objects;
};

template <class Parent>
class AttachmentMap
{
public:
    void InitializeAll(Parent *parent)
    {
        for (auto &it : m_attachments) {
            for (auto &object_it : it.second.objects) {
                WeakHandleBase &handle = object_it;

                it.second.init_fn(handle, parent);
            }
        }
    }

    template <class T>
    void Attach(Parent *parent, Handle<T> &handle, bool init_immediately)
    {
        AssertThrow(parent != nullptr);

        if (!handle) {
            return;
        }

        if (init_immediately) {
            parent->GetEngine()->template Attach<T>(handle, init_immediately);
        }

        auto it = m_attachments.template Find<NormalizedType<T>>();

        if (it == m_attachments.End()) {
            m_attachments.template Set<NormalizedType<T>>(AttachmentSet<Parent> {
                .init_fn = [](WeakHandleBase &handle, Parent *parent) {
                    auto handle_casted = handle.Lock<T>();
                    AssertThrow(handle_casted);

                    parent->GetEngine()->template Attach<T>(handle_casted);
                },
                .objects = Array<WeakHandleBase> {
                    WeakHandleBase(handle)
                }
            });

            return;
        }

        it->second.objects.PushBack(WeakHandleBase(handle));
    }

    template <class T>
    bool Detach(const Handle<T> &handle)
    {
        if (!handle) {
            return false;
        }

        auto it = m_attachments.template Find<NormalizedType<T>>();

        if (it == m_attachments.End()) {
            return false;
        }

        auto objects_it = it->second.objects.Find(handle);

        if (objects_it == it->second.objects.End()) {
            return false;
        }

        return it->second.objects.Erase(objects_it);
    }

private:
    TypeMap<AttachmentSet<Parent>> m_attachments;
};

template <class T, class ClassName>
struct StubbedClass : public Class<ClassName>
{
    using InnerType = T;

    StubbedClass() = default;
    ~StubbedClass() = default;

    /*! \brief Create a new UniquePtr instance of the type. */
    template <class ...Args>
    static UniquePtr<T> Construct(Args &&... args)
        { return UniquePtr<T>::Construct(std::forward<Args>(args)...); }
};

template <auto X>
struct ClassName
{
    using Sequence = IntegerSequenceFromString<X>;
};

#define STUB_CLASS(name) ::hyperion::v2::StubbedClass<name, ClassName<StaticString(HYP_STR(name))>>

using ComponentFlagBits = UInt;

template <class T>
struct ComponentInitInfo
{
    enum Flags : ComponentFlagBits {};

    ComponentFlagBits flags = 0x0;
};

class RenderResource { };

class EngineComponentBaseBase { };

template <class Type>
class EngineComponentBase
    : public EngineComponentBaseBase,
      public CallbackTrackable<EngineCallbacks>
{
    using InnerType = typename Type::InnerType;

public:
    using ID = typename Handle<InnerType>::ID;
    using InitInfo = ComponentInitInfo<Type>;

    static constexpr ID empty_id = ID { };

    EngineComponentBase()
        : EngineComponentBase(InitInfo { })
    {
    }

    EngineComponentBase(const String &name)
        : EngineComponentBase(name, InitInfo { })
    {
    }

    EngineComponentBase(const InitInfo &init_info)
        : EngineComponentBase(String::empty, init_info)
    {
    }

    EngineComponentBase(const String &name, const InitInfo &init_info)
        : CallbackTrackable(),
          m_name(name),
          m_init_info(init_info),
          m_id(empty_id),
          m_init_called(false),
          m_is_ready(false)
    {
    }

    EngineComponentBase(const EngineComponentBase &other) = delete;
    EngineComponentBase &operator=(const EngineComponentBase &other) = delete;
    ~EngineComponentBase() {}

    HYP_FORCE_INLINE InitInfo &GetInitInfo()
        { return m_init_info; }

    HYP_FORCE_INLINE const InitInfo &GetInitInfo() const
        { return m_init_info; }

    HYP_FORCE_INLINE bool HasFlags(ComponentFlagBits flags) const
        { return bool(m_init_info.flags & flags); }

    HYP_FORCE_INLINE void SetFlags(ComponentFlagBits flags, bool enable)
        { m_init_info.flags = enable ? (m_init_info.flags | flags) : (m_init_info.flags & ~flags); }

    HYP_FORCE_INLINE ID GetID() const
        { return m_id; }

    /* To be called from ObjectHolder<Type> */
    void SetID(const ID &id)
        { m_id = id; }

    /*! \brief Get assigned name of the object */
    const String &GetName() const { return m_name; }

    /*! \brief Set the assigned name of the object */
    void SetName(const String &name) { m_name = name; }

    /*! \brief Set the assigned name of the object */
    void SetName(String &&name) { m_name = std::move(name); }

    bool IsInitCalled() const
        { return m_init_called.load(); }

    bool IsReady() const
        { return m_is_ready.load(); }

    static inline const auto &GetClass()
        { return Type::GetInstance(); }

    /*! \brief Create a new UniquePtr instance of the type. */
    template <class ...Args>
    static inline auto Construct(Args &&... args) -> UniquePtr<InnerType>
        { return Type::Construct(std::forward<Args>(args)...); }

    /*! \brief Just a function to store that Init() has been called from a derived class
     * for book-keeping. Use to prevent adding OnInit() callbacks multiple times.
     */
    void Init()
    {
        m_init_called.store(true);
    }

protected:
    using Class = Type;
    using Base = EngineComponentBase<Type>;

    void Teardown()
    {
        RemoveFromObjectSystem();

        CallbackTrackable::Teardown();
    }

    //template <class T>
    //void Attach(Handle<T> &handle)
    //    { m_attachment_map.template Attach<T>(static_cast<InnerType *>(this), handle, m_init_called); }

    //template <class T>
    //void Detach(const Handle<T> &handle)
    //    { m_attachment_map.template Detach<T>(handle); }

    //template <class T>
    //void Detach(const typename Handle<T>::ID &id)
    //    { m_attachment_map.template Detach<T>(id); }

    void Destroy()
    {
        m_init_called.store(false);
    }
    
    void SetReady(bool is_ready)
        { m_is_ready.store(is_ready); }

    HYP_FORCE_INLINE void AssertReady() const
    {
        AssertThrowMsg(
            IsReady(),
            "Component is not in ready state; maybe Init() has not been called on it, "
            "or the component requires an event to be sent from the Engine instance to determine that "
            "it is ready to be constructed, and this event has not yet been sent.\n"
        );
    }

    HYP_FORCE_INLINE void AssertIsInitCalled() const
    {
        AssertThrowMsg(
            IsInitCalled(),
            "Component has not had Init() called on it!\n"
        );
    }
    
    ID m_id;
    String m_name;
    std::atomic_bool m_init_called;
    std::atomic_bool m_is_ready;
    InitInfo m_init_info;

    //AttachmentMap<InnerType> m_attachment_map;

private:

    void RemoveFromObjectSystem()
    {
        if (IsInitCalled()) {
            GetObjectSystem().template Release<InnerType>(GetID());
        }
    }
};

template <class Type, class WrappedType>
class EngineComponentWrapper : public EngineComponentBase<Type>
{
public:
    template <class ...Args>
    EngineComponentWrapper(Args &&... args)
        : EngineComponentBase<Type>(),
          m_wrapped(std::move(args)...),
          m_wrapped_created(false)
    {
#ifdef HYP_DEBUG_MODE
        m_wrapped_destroyed = false;
#endif
    }

    EngineComponentWrapper(const EngineComponentWrapper &other) = delete;
    EngineComponentWrapper &operator=(const EngineComponentWrapper &other) = delete;

    ~EngineComponentWrapper()
    {
        const char *type_name = typeid(WrappedType).name();

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(
            m_wrapped_destroyed,
            "Expected wrapped object of type %s to have been destroyed before destructor",
            type_name
        );
#endif

        AssertThrowMsg(
            !m_wrapped_created,
            "Expected wrapped object of type %s to be destroyed before destructor, but it was not nullptr",
            type_name
        );
    }

    WrappedType &Get() { return m_wrapped; }
    const WrappedType &Get() const { return m_wrapped; }

    /* Standard non-specialized initialization function */
    template <class ...Args>
    void Create( Args &&... args)
    {
        const char *wrapped_type_name = EngineComponentBase<Type>::GetClass().GetName();

        AssertThrowMsg(
            !m_wrapped_created,
            "Expected wrapped object of type %s to have not already been created, but it was already created.",
            wrapped_type_name
        );

        auto result = m_wrapped.Create(GetEngineDevice(), std::forward<Args>(args)...);
        AssertThrowMsg(
            result,
            "Creation of object of type %s failed.\n\tError Code: %d\n\tMessage: %s",
            wrapped_type_name,
            result.error_code,
            result.message
        );

        m_wrapped_created = true;

#ifdef HYP_DEBUG_MODE
        m_wrapped_destroyed = false;
#endif

        EngineComponentBase<Type>::Init();
    }

    /* Standard non-specialized destruction function */
    template <class ...Args>
    void Destroy( Args &&... args)
    {
        const char *wrapped_type_name = typeid(WrappedType).name();

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(
            !m_wrapped_destroyed,
            "Wrapped object of type %s was already destroyed",
            wrapped_type_name
        );
#endif

        AssertThrowMsg(
            m_wrapped_created,
            "Expected wrapped object of type %s to have been created, but it was not yet created (or it was already destroyed)",
            wrapped_type_name
        );

        auto result = m_wrapped.Destroy(GetEngineDevice(), std::move(args)...);
        AssertThrowMsg(result, "Destruction of object of type %s failed: %s", wrapped_type_name, result.message);

        m_wrapped_created = false;
        
#ifdef HYP_DEBUG_MODE
        m_wrapped_destroyed = true;
#endif

        EngineComponentBase<Type>::Destroy();
    }

protected:
    WrappedType m_wrapped;
    bool m_wrapped_created;
    bool m_wrapped_destroyed; /* for debug purposes; not really used for anything outside (as objects should be able to be destroyed then recreated) */
};

} // namespace hyperion::v2

HYP_DEF_STL_HASH(hyperion::IDBase);

#endif // !HYPERION_V2_COMPONENTS_BASE_H

