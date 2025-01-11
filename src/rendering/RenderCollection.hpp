/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_COLLECTION_HPP
#define HYPERION_RENDER_COLLECTION_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/threading/Threads.hpp>
#include <core/ID.hpp>

#include <math/Transform.hpp>

#include <scene/camera/Camera.hpp>

#include <rendering/DrawProxy.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/CullData.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

namespace platform {

template <PlatformType PLATFORM>
class Frame;

} // namespace platform

using Frame = platform::Frame<Platform::CURRENT>;

} // namespace hyperion::renderer

namespace hyperion {

class Scene;
class Camera;
class Entity;
class RenderGroup;
class RenderEnvironment;

using renderer::PushConstantData;

enum PassType : uint
{
    PASS_TYPE_INVALID = uint(-1),
    PASS_TYPE_SKYBOX = 0,
    PASS_TYPE_OPAQUE,
    PASS_TYPE_TRANSLUCENT,
    PASS_TYPE_UI,
    PASS_TYPE_MAX
};

constexpr PassType BucketToPassType(Bucket bucket)
{
    constexpr const PassType pass_type_per_bucket[uint(BUCKET_MAX)] = {
        PASS_TYPE_INVALID,     // BUCKET_SWAPCHAIN
        PASS_TYPE_INVALID,     // BUCKET_RESERVED0
        PASS_TYPE_INVALID,     // BUCKET_RESERVED1
        PASS_TYPE_OPAQUE,      // BUCKET_OPAQUE
        PASS_TYPE_TRANSLUCENT, // BUCKET_TRANSLUCENT
        PASS_TYPE_SKYBOX,      // BUCKET_SKYBOX
        PASS_TYPE_UI           // BUCKET_UI
    };

    return pass_type_per_bucket[uint(bucket)];
}

class EntityDrawCollection
{
public:
    void ClearProxyGroups();
    void RemoveEmptyProxyGroups();

    FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, PASS_TYPE_MAX> &GetProxyGroups();
    const FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, PASS_TYPE_MAX> &GetProxyGroups() const;

    RenderProxyList &GetProxyList(ThreadType);
    const RenderProxyList &GetProxyList(ThreadType) const;

    uint32 NumRenderGroups() const;

private:
    FixedArray<FlatMap<RenderableAttributeSet, Handle<RenderGroup>>, PASS_TYPE_MAX> m_proxy_groups;
    FixedArray<RenderProxyList, 2>                                                  m_proxy_lists;
};

class RenderCollector
{
public:
    struct CollectionResult
    {
        uint32  num_added_entities = 0;
        uint32  num_removed_entities = 0;
        uint32  num_changed_entities = 0;

        /*! \brief Returns true if any proxies have been added, removed or changed. */
        HYP_FORCE_INLINE bool NeedsUpdate() const
            { return num_added_entities != 0 || num_removed_entities != 0 || num_changed_entities != 0; }
    };

    RenderCollector();
    RenderCollector(const Handle<Camera> &camera);
    RenderCollector(const RenderCollector &other)                 = delete;
    RenderCollector &operator=(const RenderCollector &other)      = delete;
    RenderCollector(RenderCollector &&other) noexcept             = default;
    RenderCollector &operator=(RenderCollector &&other) noexcept  = default;
    ~RenderCollector();

    HYP_FORCE_INLINE const Handle<Camera> &GetCamera() const
        { return m_camera; }

    HYP_FORCE_INLINE void SetCamera(const Handle<Camera> &camera)
        { m_camera = camera; }

    HYP_FORCE_INLINE const WeakHandle<RenderEnvironment> &GetRenderEnvironment() const
        { return m_render_environment; }

    HYP_FORCE_INLINE void SetRenderEnvironment(const WeakHandle<RenderEnvironment> &render_environment)
        { m_render_environment = render_environment; }

    HYP_FORCE_INLINE const RC<EntityDrawCollection> &GetEntityCollection() const
        { return m_draw_collection; }

    /*! \brief Pushes an entity to the RenderCollector.
     *  \param entity The entity the proxy is used for
     *  \param proxy A RenderProxy associated with the entity
     */
    void PushEntityToRender(
        ID<Entity> entity,
        const RenderProxy &proxy
    );

    /*! \brief Creates RenderGroups needed for rendering the Entity objects.
     *  Call after calling CollectEntities() on Scene. */
    CollectionResult PushUpdatesToRenderThread(
        const FramebufferRef &framebuffer = nullptr,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    );

    void CollectDrawCalls(
        Frame *frame,
        const Bitset &bucket_bits,
        const CullData *cull_data
    );

    void ExecuteDrawCalls(
        Frame *frame,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    void ExecuteDrawCalls(
        Frame *frame,
        const FramebufferRef &framebuffer,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    /*! \brief Perform a full reset, when this is not needed anymore. */
    void Reset();

protected:
    void ExecuteDrawCalls(
        Frame *frame,
        const Handle<Camera> &camera,
        const FramebufferRef &framebuffer,
        const Bitset &bucket_bits,
        const CullData *cull_data = nullptr,
        PushConstantData push_constant = { }
    ) const;

    Handle<Camera>                  m_camera;
    WeakHandle<RenderEnvironment>   m_render_environment;
    RC<EntityDrawCollection>        m_draw_collection;
};

} // namespace hyperion

#endif