#ifndef HYPERION_V2_RENDER_OBJECT_HPP
#define HYPERION_V2_RENDER_OBJECT_HPP

#include <core/Containers.hpp>
#include <core/IDCreator.hpp>
#include <core/lib/ValueStorage.hpp>
#include <Threads.hpp>
#include <Constants.hpp>
#include <Types.hpp>

#include <mutex>
#include <atomic>

namespace hyperion {
namespace v2 {

template <class T>
struct RenderObjectDefinition;

template <class T>
constexpr bool has_render_object_defined = implementation_exists<RenderObjectDefinition<T>>;

template <class T>
class RenderObjectContainer
{
public:
    static constexpr SizeType max_size = RenderObjectDefinition<T>::max_size;

    static_assert(has_render_object_defined<T>, "T is not a render object!");

    struct Instance
    {
        ValueStorage<T> storage;
        std::atomic<UInt16> ref_count;
        bool has_value;

        Instance()
            : ref_count(0),
              has_value(false)
        {
        }

        Instance(const Instance &other) = delete;
        Instance &operator=(const Instance &other) = delete;
        Instance(Instance &&other) noexcept = delete;
        Instance &operator=(Instance &&other) = delete;

        ~Instance()
        {
            if (HasValue()) {
                storage.Destruct();
            }
        }

        UInt16 GetRefCount() const
        {
            return ref_count.load();
        }

        template <class ...Args>
        T *Construct(Args &&... args)
        {
            AssertThrow(!HasValue());

            T *ptr = storage.Construct(std::forward<Args>(args)...);

            has_value = true;

            return ptr;
        }

        HYP_FORCE_INLINE void IncRef()
            { ref_count.fetch_add(1, std::memory_order_relaxed); }

        UInt DecRef()
        {
            AssertThrow(GetRefCount() != 0);

            UInt16 count;

            if ((count = ref_count.fetch_sub(1)) == 1) {
                storage.Destruct();
                has_value = false;
            }

            return UInt(count) - 1;
        }

        HYP_FORCE_INLINE T &Get()
        {
            AssertThrow(HasValue());

            return storage.Get();
        }

    private:
        HYP_FORCE_INLINE bool HasValue() const
            { return has_value; }
    };

    RenderObjectContainer()
        : m_size(0)
    {
    }

    RenderObjectContainer(const RenderObjectContainer &other) = delete;
    RenderObjectContainer &operator=(const RenderObjectContainer &other) = delete;

    ~RenderObjectContainer() = default;

    HYP_FORCE_INLINE UInt NextIndex()
    {
        const UInt index = IDCreator::ForType<T>().NextID() - 1;

        AssertThrowMsg(
            index < RenderObjectDefinition<T>::max_size,
            "Maximum number of RenderObject type allocated! Maximum: %llu\n",
            RenderObjectDefinition<T>::max_size
        );

        return index;
    }

    HYP_FORCE_INLINE void IncRef(UInt index)
    {
        m_data[index].IncRef();
    }

    HYP_FORCE_INLINE void DecRef(UInt index)
    {
        if (m_data[index].DecRef() == 0) {
            IDCreator::ForType<T>().FreeID(index + 1);
        }
    }

    HYP_FORCE_INLINE UInt16 GetRefCount(UInt index)
    {
        return m_data[index].GetRefCount();
    }

    HYP_FORCE_INLINE T &Get(UInt index)
    {
        return m_data[index].Get();
    }
    
    template <class ...Args>
    HYP_FORCE_INLINE void ConstructAtIndex(UInt index, Args &&... args)
    {
        m_data[index].Construct(std::forward<Args>(args)...);
    }

private:
    HeapArray<Instance, max_size> m_data;
    SizeType m_size;
};

template <class T>
class RenderObjectHandle;

class RenderObjects
{
public:
    template <class T>
    static RenderObjectContainer<T> &GetRenderObjectContainer()
    {
        static_assert(has_render_object_defined<T>, "Not a valid render object");

        static RenderObjectContainer<T> container;

        return container;
    }

    template <class T, class ...Args>
    static RenderObjectHandle<T> Make(Args &&... args)
    {
        auto &container = GetRenderObjectContainer<T>();

        const UInt index = container.NextIndex();

        container.ConstructAtIndex(
            index,
            std::forward<Args>(args)...
        );

        return RenderObjectHandle<T>::FromIndex(index + 1);
    }
};

template <class T>
class RenderObjectHandle
{
    static_assert(has_render_object_defined<T>, "Not a valid render object");

    UInt index;

    RenderObjectContainer<T> *_container = &RenderObjects::GetRenderObjectContainer<T>();

public:

    static RenderObjectHandle FromIndex(UInt index)
    {
        RenderObjectHandle handle;
        handle.index = index;
        
        if (index != 0) {
            handle._container->IncRef(index - 1);
        }

        AssertThrow(handle.GetRefCount() != 0);

        return handle;
    }

    RenderObjectHandle()
        : index(0)
    {
    }

    RenderObjectHandle(const RenderObjectHandle &other)
        : index(other.index)
    {
        if (index != 0) {
            _container->IncRef(index - 1);
        }
    }

    RenderObjectHandle &operator=(const RenderObjectHandle &other)
    {
        if (index != 0) {
            _container->DecRef(index - 1);
        }

        index = other.index;

        if (index != 0) {
            _container->IncRef(index - 1);
        }

        return *this;
    }

    RenderObjectHandle(RenderObjectHandle &&other) noexcept
        : index(other.index)
    {
        other.index = 0;
    }

    RenderObjectHandle &operator=(RenderObjectHandle &&other) noexcept
    {
        if (index != 0) {
            _container->DecRef(index - 1);
        }

        index = other.index;
        other.index = 0;

        return *this;
    }

    ~RenderObjectHandle()
    {
        if (index != 0) {
            _container->DecRef(index - 1);
        }
    }

    T *operator->() const
        { return Get(); }

    T &operator*()
        { return *Get(); }

    const T &operator*() const
        { return *Get(); }

    bool operator!() const
        { return !IsValid(); }

    explicit operator bool() const
        { return IsValid(); }

    bool operator==(std::nullptr_t) const
        { return !IsValid(); }

    bool operator!=(std::nullptr_t) const
        { return IsValid(); }

    bool operator==(const RenderObjectHandle &other) const
        { return index == other.index; }

    bool operator!=(const RenderObjectHandle &other) const
        { return index == other.index; }

    bool operator<(const RenderObjectHandle &other) const
        { return index < other.index; }

    bool IsValid() const
        { return index != 0; }

    UInt16 GetRefCount() const
    {
        return index == 0 ? 0 : _container->GetRefCount(index - 1);
    }

    T *Get() const
    {
        if (index == 0) {
            return nullptr;
        }
        
        return &_container->Get(index - 1);
    }

    void Reset()
    {
        if (index != 0) {
            _container->DecRef(index - 1);
        }

        index = 0;
    }

    operator T *() const
        { return Get(); }

    template <class ...Args>
    [[nodiscard]] static RenderObjectHandle Construct(Args &&... args)
    {
        return RenderObjects::Make<T>(std::forward<Args>(args)...);
    }
};

} // namespace v2

#define DEF_RENDER_OBJECT(T, _max_size) \
    namespace renderer { \
    class T; \
    } \
    \
    namespace v2 { \
    template <> \
    struct RenderObjectDefinition< renderer::T > \
    { \
        static constexpr SizeType max_size = (_max_size); \
    }; \
    using T##Ref = RenderObjectHandle< renderer::T >; \
    } // namespace v2

DEF_RENDER_OBJECT(CommandBuffer,       2048);
DEF_RENDER_OBJECT(ShaderProgram,       2048);
DEF_RENDER_OBJECT(GPUBuffer,           65536);
DEF_RENDER_OBJECT(Image,               16384);
DEF_RENDER_OBJECT(ImageView,           65536);
DEF_RENDER_OBJECT(Sampler,             16384);
DEF_RENDER_OBJECT(RaytracingPipeline,  128);
DEF_RENDER_OBJECT(DescriptorSet,       4096);

#undef DEF_RENDER_OBJECT

} // namespace hyperion

#endif