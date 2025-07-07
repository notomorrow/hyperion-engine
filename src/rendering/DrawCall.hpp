/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/object/ObjId.hpp>
#include <core/Handle.hpp>

#include <core/memory/UniquePtr.hpp>

#include <rendering/GpuBufferHolderMap.hpp>

#include <rendering/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderGroup;
class RenderProxyMesh;
struct DrawCommandData;
class IndirectDrawState;
class GpuBufferHolderBase;
struct MeshInstanceData;
class RenderMesh;

extern HYP_API GpuBufferHolderMap* GetGpuBufferHolderMap();

static constexpr uint32 maxEntitiesPerInstanceBatch = 60;

struct alignas(16) EntityInstanceBatch
{
    uint32 batchIndex;
    uint32 numEntities;
    uint32 _pad0;
    uint32 _pad1;

    uint32 indices[maxEntitiesPerInstanceBatch];
    Matrix4 transforms[maxEntitiesPerInstanceBatch];
};

static_assert(sizeof(EntityInstanceBatch) == 4096);

/*! \brief Unique identifier for a draw call based on Mesh Id and Material Id.
 *  \details This struct is used to uniquely identify a draw call in the rendering system.
 *  It combines the mesh Id and material Id into a single 64-bit value, where the lower 32 bits
 *  represent the mesh Id and the upper 32 bits represent the material Id. */
struct DrawCallID
{
    static constexpr uint64 meshMask = uint64(0xFFFFFFFF);
    static constexpr uint64 materialMask = uint64(0xFFFFFFFF) << 32;

    uint64 value;

    constexpr DrawCallID()
        : value(0)
    {
    }

    constexpr DrawCallID(ObjId<Mesh> meshId)
        : value(meshId.Value())
    {
    }

    constexpr DrawCallID(ObjId<Mesh> meshId, ObjId<Material> materialId)
        : value(uint64(meshId.Value()) | (uint64(materialId.Value()) << 32))
    {
    }

    HYP_FORCE_INLINE constexpr operator uint64() const
    {
        return value;
    }

    HYP_FORCE_INLINE bool operator==(const DrawCallID& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE bool operator!=(const DrawCallID& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE bool HasMaterial() const
    {
        return bool(value & (uint64(~0u) << 32));
    }

    HYP_FORCE_INLINE constexpr uint64 Value() const
    {
        return value;
    }
};

/*! \brief Base struct for all draw calls.
 *  \details This struct contains the common data for all draw calls, such as the Id, render mesh, material, and skeleton.
 *  It is used as a base class for both `DrawCall` and `InstancedDrawCall`. */
struct DrawCallBase
{
    DrawCallID id;

    RenderMesh* renderMesh = nullptr;
    Material* material = nullptr;
    Skeleton* skeleton = nullptr;

    uint32 drawCommandIndex = 0;
};

/*! \brief Represents a draw call for a single entity.
 *  \details This is used for non-instanced draw calls, where each entity has its own draw call.
 *  The `entityId` is the Id of the entity that this draw call represents. */
struct DrawCall : DrawCallBase
{
    ObjId<Entity> entityId;
};

/*! \brief Represents a draw call for multiple entities sharing the same mesh and material.
 *  \details This is used for instanced draw calls, where multiple entities share the same mesh and material.
 *  The `batch` is the entity instance batch that contains the instances of the entities. */
struct InstancedDrawCall : DrawCallBase
{
    EntityInstanceBatch* batch = nullptr;

    uint32 count = 0;
    ObjId<Entity> entityIds[maxEntitiesPerInstanceBatch];
};

/// TODO: Refactor to a basic desc struct for Batch size info,
/// and use g_renderGlobalState to acquire batch holder / acquire and release batches.
class IDrawCallCollectionImpl
{
public:
    virtual ~IDrawCallCollectionImpl() = default;

    virtual SizeType GetStructSize() const = 0;
    virtual SizeType GetStructAlignment() const = 0;
    virtual EntityInstanceBatch* AcquireBatch() const = 0;
    virtual void ReleaseBatch(EntityInstanceBatch* batch) const = 0;
    virtual GpuBufferHolderBase* GetEntityInstanceBatchHolder() const = 0;
};

struct DrawCallCollection
{
    DrawCallCollection()
        : impl(nullptr),
          renderGroup(nullptr)
    {
    }

    DrawCallCollection(IDrawCallCollectionImpl* impl, RenderGroup* renderGroup)
        : impl(impl),
          renderGroup(renderGroup)
    {
    }

    DrawCallCollection(const DrawCallCollection& other) = delete;
    DrawCallCollection& operator=(const DrawCallCollection& other) = delete;

    DrawCallCollection(DrawCallCollection&& other) noexcept;
    DrawCallCollection& operator=(DrawCallCollection&& other) noexcept;

    ~DrawCallCollection();

    void PushRenderProxy(DrawCallID id, const RenderProxyMesh& renderProxy);
    void PushRenderProxyInstanced(EntityInstanceBatch* batch, DrawCallID id, const RenderProxyMesh& renderProxy);

    EntityInstanceBatch* TakeDrawCallBatch(DrawCallID id);

    void ResetDrawCalls();

    /*! \brief Push \ref{numInstances} instances of the given entity into an entity instance batch.
     *  If not all instances could be pushed to the given draw call's batch, a positive number will be returned.
     *  Otherwise, zero will be returned. */
    uint32 PushEntityToBatch(InstancedDrawCall& drawCall, ObjId<Entity> entity, const MeshInstanceData& meshInstanceData, uint32 numInstances, uint32 instanceOffset);

    IDrawCallCollectionImpl* impl;

    RenderGroup* renderGroup;

    Array<DrawCall> drawCalls;
    Array<InstancedDrawCall> instancedDrawCalls;

    // Map from draw call Id to index in instancedDrawCalls
    HashMap<uint64, Array<SizeType>> indexMap;
};

template <class EntityInstanceBatchType>
class DrawCallCollectionImpl final : public IDrawCallCollectionImpl
{
public:
    static_assert(std::is_base_of_v<EntityInstanceBatch, EntityInstanceBatchType>, "EntityInstanceBatchType must be a derived struct type of EntityInstanceBatch");
    static_assert(offsetof(EntityInstanceBatchType, indices) == 16, "offsetof for member `indices` of the derived EntityInstanceBatch type must be 16 or shader calculations will be incorrect!");

    DrawCallCollectionImpl()
        : m_entityInstanceBatches(GetGpuBufferHolderMap()->GetOrCreate<EntityInstanceBatchType>())
    {
    }

    virtual ~DrawCallCollectionImpl() override = default;

    virtual SizeType GetStructSize() const override
    {
        return sizeof(EntityInstanceBatchType);
    }

    virtual SizeType GetStructAlignment() const override
    {
        return alignof(EntityInstanceBatchType);
    }

    virtual EntityInstanceBatch* AcquireBatch() const override
    {
        EntityInstanceBatchType* batch;
        const uint32 batchIndex = m_entityInstanceBatches->AcquireIndex(&batch);

        batch->batchIndex = batchIndex;

        return batch;
    }

    virtual void ReleaseBatch(EntityInstanceBatch* batch) const override
    {
        m_entityInstanceBatches->ReleaseIndex(batch->batchIndex);
    }

    virtual GpuBufferHolderBase* GetEntityInstanceBatchHolder() const override
    {
        // Need to use reinterpret_cast because GpuBufferHolder is forward declared here
        return reinterpret_cast<GpuBufferHolderBase*>(m_entityInstanceBatches);
    }

private:
    GpuBufferHolder<EntityInstanceBatchType, GpuBufferType::SSBO>* m_entityInstanceBatches;
};

extern HYP_API IDrawCallCollectionImpl* GetDrawCallCollectionImpl(TypeId typeId);
extern HYP_API IDrawCallCollectionImpl* SetDrawCallCollectionImpl(TypeId typeId, UniquePtr<IDrawCallCollectionImpl>&& impl);

template <class EntityInstanceBatchType>
IDrawCallCollectionImpl* GetOrCreateDrawCallCollectionImpl()
{
    if (IDrawCallCollectionImpl* impl = GetDrawCallCollectionImpl(TypeId::ForType<EntityInstanceBatchType>()))
    {
        return impl;
    }

    return SetDrawCallCollectionImpl(TypeId::ForType<EntityInstanceBatchType>(), MakeUnique<DrawCallCollectionImpl<EntityInstanceBatchType>>());
}

} // namespace hyperion
