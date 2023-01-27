#ifndef HYPERION_V2_DRAW_CALL_HPP
#define HYPERION_V2_DRAW_CALL_HPP

#include <core/Containers.hpp>
#include <Constants.hpp>
#include <core/ID.hpp>
#include <core/lib/AtomicSemaphore.hpp>
#include <rendering/Buffers.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

#include <rendering/ShaderGlobals.hpp>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::GPUBuffer;

class Engine;
class Mesh;
class Material;
class Skeleton;

struct DrawCommandData;
class IndirectDrawState;

enum ResourceUsageType : UInt
{
    RESOURCE_USAGE_TYPE_INVALID = UInt(-1),
    RESOURCE_USAGE_TYPE_MESH = 0,
    RESOURCE_USAGE_TYPE_MATERIAL,
    RESOURCE_USAGE_TYPE_SKELETON,
    RESOURCE_USAGE_TYPE_MAX
};

template <class T>
struct ResourceUsageTypeMap { };

template <>
struct ResourceUsageTypeMap<Mesh>
    { enum { value = RESOURCE_USAGE_TYPE_MESH }; };

template <>
struct ResourceUsageTypeMap<Material>
    { enum { value = RESOURCE_USAGE_TYPE_MATERIAL }; };

template <>
struct ResourceUsageTypeMap<Skeleton>
    { enum { value = RESOURCE_USAGE_TYPE_SKELETON }; };

// holds a handle for any resource needed in
// rendering, so that objects like meshes and materials
// do not get destroyed while being rendered.
// rather than passing a Handle<T> around,
// you only need to use the ID.
class RenderResourceManager
{
private:
    struct ResourceUsageMapBase { };

    template <class T>
    struct ResourceUsageMap : ResourceUsageMapBase
    {
        HashMap<ID<T>, Handle<T>> handles;
        Bitset usage_bits;

        Handle<T> TakeUsage(ID<T> id)
        {
            if (!usage_bits.Test(id.Value())) {
                return Handle<T>::empty;
            }

            auto it = handles.Find(id);
            AssertThrow(it != handles.End());

            usage_bits.Set(id.Value(), false);

            return std::move(it->value);
        }
    };

    FixedArray<UniquePtr<ResourceUsageMapBase>, RESOURCE_USAGE_TYPE_MAX> m_resource_usage_maps;

public:

    RenderResourceManager() = default;
    RenderResourceManager(const RenderResourceManager &other) = delete;
    RenderResourceManager &operator=(const RenderResourceManager &other) = delete;
    RenderResourceManager(RenderResourceManager &&other) noexcept = default;
    RenderResourceManager &operator=(RenderResourceManager &&other) = default;
    ~RenderResourceManager() = default;

    template <class T>
    ResourceUsageMap<T> *GetResourceUsageMap()
    {
        ResourceUsageMapBase *ptr = m_resource_usage_maps[ResourceUsageTypeMap<T>::value].Get();

        if (ptr == nullptr) {
            // UniquePtr of derived class
            m_resource_usage_maps[ResourceUsageTypeMap<T>::value] = UniquePtr<ResourceUsageMap<T>>::Construct();

            ptr = m_resource_usage_maps[ResourceUsageTypeMap<T>::value].Get();
        }

        return static_cast<ResourceUsageMap<T> *>(ptr);
    }

    template <class T>
    Handle<T> TakeResourceUsage(ID<T> id)
    {
        if (!id) {
            return Handle<T>::empty;
        }

        ResourceUsageMap<T> *ptr = GetResourceUsageMap<T>();

        return ptr->TakeUsage(id);
    }

    template <class T>
    void SetIsUsed(ID<T> id, Handle<T> &&handle, bool is_used)
    {
        if (!id) {
            return;
        }

        ResourceUsageMap<T> *ptr = GetResourceUsageMap<T>();

        if (is_used != ptr->usage_bits.Test(id.Value())) {
            ptr->usage_bits.Set(id.Value(), is_used);

            if (is_used) {
                if (!handle) {
                    DebugLog(
                        LogType::Debug,
                        "Alloc Handle %s (ID: #%u) in thread: %s\n",
                        handle.GetClassNameString(),
                        id.Value(),
                        Threads::CurrentThreadID().name.Data()
                    );

                    handle = Handle<T>(id);
                }

                ptr->handles.Set(id, std::move(handle));
            } else {
                ptr->handles.Set(id, Handle<T>::empty);
            }
        }
    }

    template <class T>
    void SetIsUsed(ID<T> id, bool is_used)
    {
        SetIsUsed<T>(id, Handle<T>(), is_used);
    }

    template <class T>
    bool IsUsed(ID<T> id) const
    {
        if (!id) {
            return false;
        }

        const ResourceUsageMap<T> *ptr = static_cast<const ResourceUsageMap<T> *>(m_resource_usage_maps[ResourceUsageTypeMap<T>::value].Get());

        if (ptr == nullptr) {
            return false;
        }

#ifndef HYP_DEBUG_MODE
        return ptr->usage_bits.Test(id.Value())

#else
        auto it = ptr->handles.Find(id);

        if (!ptr->usage_bits.Test(id.Value())) {
            AssertThrow(it == ptr->handles.End() || !it->value.IsValid());

            return false;
        }

        AssertThrow(it != ptr->handles.End());
        AssertThrow(it->value.IsValid());

        return true;
#endif
    }
};

struct DrawCallID
{
    static_assert(sizeof(ID<Mesh>) == 4, "Handle ID should be 32 bit for DrawCallID to be able to store two IDs.");

    UInt64 value;

    DrawCallID()
        : value(0)
    {
    }

    DrawCallID(ID<Mesh> mesh_id)
        : value(mesh_id.Value())
    {
    }

    DrawCallID(ID<Mesh> mesh_id, ID<Material> material_id)
        : value(mesh_id.Value() | (UInt64(material_id.Value()) << 32))
    {
    }

    bool operator==(const DrawCallID &other) const
        { return value == other.value; }

    bool operator!=(const DrawCallID &other) const
        { return value != other.value; }

    bool HasMaterial() const
        { return bool((UInt64(~0u) << 32) & value); }

    operator UInt64() const
        { return value; }

    UInt64 Value() const
        { return value; }
};

struct DrawCall
{
    static constexpr bool unique_per_material = !use_indexed_array_for_object_data;

    DrawCallID id;
    BufferTicket<EntityInstanceBatch> batch_index;
    SizeType draw_command_index;

    ID<Mesh> mesh_id;
    ID<Material> material_id;
    ID<Skeleton> skeleton_id;

    UInt entity_id_count;
    ID<Entity> entity_ids[max_entities_per_instance_batch];

    Mesh *mesh;

    UInt packed_data[4];
};

struct DrawCallCollection
{
    Array<DrawCall> draw_calls;
    HashMap<UInt64 /* DrawCallID */, Array<SizeType>> index_map;

    DrawCallCollection() = default;
    DrawCallCollection(const DrawCallCollection &other) = delete;
    DrawCallCollection &operator=(const DrawCallCollection &other) = delete;

    DrawCallCollection(DrawCallCollection &&other) noexcept;
    DrawCallCollection &operator=(DrawCallCollection &&other) noexcept;

    ~DrawCallCollection();

    void PushDrawCall(BufferTicket<EntityInstanceBatch> batch_index, DrawCallID id, const EntityDrawProxy &entity);
    DrawCall *TakeDrawCall(DrawCallID id);
    void Reset();
};

} // namespace hyperion::v2

#endif
