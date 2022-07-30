#ifndef HYPERION_V2_SAFE_DELETER_H
#define HYPERION_V2_SAFE_DELETER_H

#include <core/Containers.hpp>

#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Shader.hpp>
#include <animation/Skeleton.hpp>

#include <Types.hpp>

#include <mutex>
#include <tuple>
#include <atomic>

namespace hyperion::v2 {

class Engine;

using RenderableDeletionMaskBits = UInt;

enum RenderableDeletionMask : RenderableDeletionMaskBits {
    RENDERABLE_DELETION_NONE      = 0,
    RENDERABLE_DELETION_TEXTURES  = 1 << 0,
    RENDERABLE_DELETION_MATERIALS = 1 << 1,
    RENDERABLE_DELETION_MESHES    = 1 << 2,
    RENDERABLE_DELETION_SKELETONS = 1 << 3,
    RENDERABLE_DELETION_SHADERS   = 1 << 4
};

class SafeDeleter {
public:
    void SafeReleaseRenderResource(Ref<Texture> &&resource)
    {
        std::lock_guard guard(m_render_resource_deletion_mutex);
        
        std::get<DynArray<RenderableDeletionEntry<Texture>>>(m_render_resource_deletion_queue_items).PushBack({
            .ref     = std::move(resource),
            .deleter = [](Ref<Texture> &&ref) {
                ref.Reset();
            }
        });

        m_render_resource_deletion_flag |= RENDERABLE_DELETION_TEXTURES;
    }
    
    void SafeReleaseRenderResource(Ref<Mesh> &&resource)
    {
        std::lock_guard guard(m_render_resource_deletion_mutex);
        
        std::get<DynArray<RenderableDeletionEntry<Mesh>>>(m_render_resource_deletion_queue_items).PushBack({
            .ref     = std::move(resource),
            .deleter = [](Ref<Mesh> &&ref) {
                ref.Reset();
            }
        });

        m_render_resource_deletion_flag |= RENDERABLE_DELETION_MESHES;
    }
    
    void SafeReleaseRenderResource(Ref<Skeleton> &&resource)
    {
        std::lock_guard guard(m_render_resource_deletion_mutex);
        
        std::get<DynArray<RenderableDeletionEntry<Skeleton>>>(m_render_resource_deletion_queue_items).PushBack({
            .ref     = std::move(resource),
            .deleter = [](Ref<Skeleton> &&ref) {
                ref.Reset();
            }
        });

        m_render_resource_deletion_flag |= RENDERABLE_DELETION_SKELETONS;
    }
    
    void SafeReleaseRenderResource(Ref<Shader> &&resource)
    {
        std::lock_guard guard(m_render_resource_deletion_mutex);
        
        std::get<DynArray<RenderableDeletionEntry<Shader>>>(m_render_resource_deletion_queue_items).PushBack({
            .ref     = std::move(resource),
            .deleter = [](Ref<Shader> &&ref) {
                ref.Reset();
            }
        });

        m_render_resource_deletion_flag |= RENDERABLE_DELETION_SHADERS;
    }

    template <class T>
    struct RenderableDeletionEntry {
        static_assert(std::is_base_of_v<RenderResource, T>, "T must be a derived class of RenderResource");

        using Deleter = std::add_pointer_t<void(Ref<T> &&)>;

        UInt    cycles_remaining = max_frames_in_flight;
        Ref<T>  ref;
        Deleter deleter;
    };

    void PerformEnqueuedDeletions();

    // returns true if all were completed
    template <class T>
    bool DeleteEnqueuedItemsOfType()
    {
        auto &queue = std::get<DynArray<RenderableDeletionEntry<T>>>(m_render_resource_deletion_queue_items);

        for (auto it = queue.Begin(); it != queue.End();) {
            auto &front = queue.Front();

            if (!--front.cycles_remaining) {
                front.deleter(std::move(front.ref));
                it = queue.Erase(it);
            } else {
                ++it;
            }
        }

        return queue.Empty();
    }

    std::tuple<
        DynArray<RenderableDeletionEntry<Texture>>,
        DynArray<RenderableDeletionEntry<Mesh>>,
        DynArray<RenderableDeletionEntry<Skeleton>>,
        DynArray<RenderableDeletionEntry<Shader>>
    > m_render_resource_deletion_queue_items;
    
    std::mutex                              m_render_resource_deletion_mutex;
    std::atomic<RenderableDeletionMaskBits> m_render_resource_deletion_flag{RENDERABLE_DELETION_NONE};
};

} // namespace hyperion::v2

#endif