#ifndef HYPERION_V2_DRAW_PROXY_H
#define HYPERION_V2_DRAW_PROXY_H

#include "Base.hpp"

#include <math/BoundingBox.hpp>
#include <math/Vector4.hpp>

#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <memory>

namespace hyperion {

class Camera;

} // namespace hyperion

namespace hyperion::v2 {

using renderer::IndirectDrawCommand;
using renderer::IndirectBuffer;
using renderer::StorageBuffer;
using renderer::StagingBuffer;
using renderer::Frame;
using renderer::Result;
using renderer::Extent2D;

class Mesh;
class Material;
class Engine;
class Spatial;

struct alignas(16) ObjectInstance {
    UInt32  entity_id;
    UInt32  draw_command_index;
    UInt32  batch_index;
    UInt32  num_indices;
    Vector4 aabb_max;
    Vector4 aabb_min;
};

template <class T>
struct Drawable {};

template <>
struct Drawable<STUB_CLASS(Spatial)> {
    // rendering objects (sent from game thread to
    // rendering thread when updates are enqueued)
    // engine will hold onto RenderResources like Mesh and Material
    // for at least {max_frames_in_flight} frames, so we will have
    // rendered the Drawable before the ptr is invalidated.
    Mesh           *mesh     = nullptr;
    Material       *material = nullptr;

    IDBase         entity_id,
                   scene_id,
                   mesh_id,
                   material_id,
                   skeleton_id;

    BoundingBox    bounding_box;

    // object instance in GPU
    ObjectInstance object_instance;
};

using EntityDrawProxy = Drawable<STUB_CLASS(Spatial)>;

template <>
struct Drawable<STUB_CLASS(Camera)> {
    Matrix4  view;
    Matrix4  projection;
    Vector3  position;
    Vector3  direction;
    Extent2D dimensions;
    Float    clip_near;
    Float    clip_far;
    Float    fov;
};

using CameraDrawProxy = Drawable<STUB_CLASS(Camera)>;

template <class T>
class HasDrawProxy {
public:
    HasDrawProxy()
        : m_drawable{}
    {
    }

    HasDrawProxy(const Drawable<T> &drawable)
        : m_drawable(drawable)
    {
    }

    HasDrawProxy(Drawable<T> &&drawable)
        : m_drawable(std::move(drawable))
    {
    }

    template <class ...Args>
    HasDrawProxy(Args &&... args)
        : m_drawable(std::forward<Args>(args)...)
    {
    }

    HasDrawProxy(const HasDrawProxy &other)
        : m_drawable(other.m_drawable)
    {
    }

    HasDrawProxy &operator=(const HasDrawProxy &other)
    {
        m_drawable = other.m_drawable;
        return *this;
    }

    HasDrawProxy(HasDrawProxy &&other) noexcept
        : m_drawable(std::move(other.m_drawable))
    {
    }

    HasDrawProxy &operator=(HasDrawProxy &&other) noexcept
    {
        m_drawable = std::move(other.m_drawable);
        return *this;
    }

    ~HasDrawProxy() = default;

    /*! \brief Get the DrawProxy for this object. Only call from render thread. */
    const Drawable<T> &GetDrawProxy() const { return m_drawable; }

protected:
    /*! Enqueue an update /for/ the render thread to update the draw proxy.
        Cannot set it directly in a thread-safe manner if not on the render thread,
        so use this.
    */
    template <class EngineImpl>
    void EnqueueDrawProxyUpdate(EngineImpl *engine, Drawable<T> &&drawable)
    {
        engine->render_scheduler.Enqueue([this, _drawable = std::move(drawable)](...) mutable {
            // just update drawable object on render thread
            HasDrawProxy::m_drawable = _drawable;

            HYPERION_RETURN_OK;
        });
    }

    // Only touch from render thread.
    // Update this when updates are enqueued, so just update
    // the shader data state to dirty to refresh this.
    Drawable<T> m_drawable;
};

} // namespace hyperion::v2

#endif