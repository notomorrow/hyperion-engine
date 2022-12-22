#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderCommands.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

FixedArray<RenderObjectDeleter::DeletionQueueBase *, RenderObjectDeleter::max_queues + 1> RenderObjectDeleter::queues = { };
std::atomic_uint16_t RenderObjectDeleter::queue_index = { 0 };


struct RENDER_COMMAND(RemoveTextureFromBindlessStorage) : RenderCommand
{
    ID<Texture> id;

    RENDER_COMMAND(RemoveTextureFromBindlessStorage)(ID<Texture> id) : id(id)
    {
    }

    virtual Result operator()()
    {
        Engine::Get()->GetRenderData()->textures.RemoveResource(id);

        HYPERION_RETURN_OK;
    }
};

void SafeDeleter::PerformEnqueuedDeletions()
{
    if (auto deletion_flags = m_render_resource_deletion_flag.load()) {
        std::lock_guard guard(m_render_resource_deletion_mutex);

        if (deletion_flags & RENDERABLE_DELETION_BUFFERS_OR_IMAGES) {
            if (DeleteEnqueuedBuffersAndImages()) {
                deletion_flags &= ~RENDERABLE_DELETION_BUFFERS_OR_IMAGES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_TEXTURES) {
            if (DeleteEnqueuedHandlesOfType<Texture>()) {
                deletion_flags &= ~RENDERABLE_DELETION_TEXTURES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_MESHES) {
            if (DeleteEnqueuedHandlesOfType<Mesh>()) {
                deletion_flags &= ~RENDERABLE_DELETION_MESHES;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SKELETONS) {
            if (DeleteEnqueuedHandlesOfType<Skeleton>()) {
                deletion_flags &= ~RENDERABLE_DELETION_SKELETONS;
            }
        }

        if (deletion_flags & RENDERABLE_DELETION_SHADERS) {
            if (DeleteEnqueuedHandlesOfType<Shader>()) {
                deletion_flags &= ~RENDERABLE_DELETION_SHADERS;
            }
        }

        m_render_resource_deletion_flag.store(deletion_flags);
    }
}

void SafeDeleter::ForceReleaseAll()
{
    if (auto deletion_flags = m_render_resource_deletion_flag.load()) {
        std::lock_guard guard(m_render_resource_deletion_mutex);

        if (deletion_flags & RENDERABLE_DELETION_BUFFERS_OR_IMAGES) {
            ForceDeleteBuffersAndImages();
            deletion_flags &= ~RENDERABLE_DELETION_BUFFERS_OR_IMAGES;
        }

        if (deletion_flags & RENDERABLE_DELETION_TEXTURES) {
            ForceDeleteHandlesOfType<Texture>();
            deletion_flags &= ~RENDERABLE_DELETION_TEXTURES;
        }

        if (deletion_flags & RENDERABLE_DELETION_MESHES) {
            ForceDeleteHandlesOfType<Mesh>();
            deletion_flags &= ~RENDERABLE_DELETION_MESHES;
        }

        if (deletion_flags & RENDERABLE_DELETION_SKELETONS) {
            ForceDeleteHandlesOfType<Skeleton>();
            deletion_flags &= ~RENDERABLE_DELETION_SKELETONS;
        }

        if (deletion_flags & RENDERABLE_DELETION_SHADERS) {
            ForceDeleteHandlesOfType<Shader>();
            deletion_flags &= ~RENDERABLE_DELETION_SHADERS;
        }

        m_render_resource_deletion_flag.store(deletion_flags);
    }
}

void SafeDeleter::EnqueueTextureBindlessStorageRemoval(ID<Texture> id)
{
    if (Engine::Get()->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
        PUSH_RENDER_COMMAND(RemoveTextureFromBindlessStorage, id);
    }
}

} // namespace hyperion::v2