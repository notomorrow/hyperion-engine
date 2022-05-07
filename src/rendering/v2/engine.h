#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include "asset/assets.h"

#include "components/post_fx.h"
#include "components/render_list.h"
#include "components/deferred.h"
#include "components/shadows.h"
#include "components/resources.h"
#include "scene/octree.h"

#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_semaphore.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <util/enum_options.h>

#include <memory>
#include <mutex>
#include <stack>

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;
using renderer::Semaphore;
using renderer::SemaphoreChain;
using renderer::Image;
using renderer::StorageBuffer;

/* Current descriptor / attachment layout */

/*
 * | ====== Set 0 ====== | ====== Set 1 ====== | ====== Set 2 ====== | ====== Set 3 ====== | ====== Set 4 ====== |
 * | (UNUSED)            | GBuffer: color      | Scene data SSBO     | Material data SSBO  | Bindless textures   |
 * |                     | Gbuffer: normals    | empty               | Object data SSBO    | empty               |
 * |                     | Gbuffer: position   | empty               | Skeleton data SSBO  | empty               |
 * |                     | Gbuffer: depth      | empty               | empty               | empty               |
 * |                     | Deferred result     | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | Post effect 0       | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | Shadow map          | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | empty               | empty               | empty               | empty               |
 * |                     | Image storage test  | empty               | empty               | empty               |
 */

struct RenderBindings {
    std::stack<Scene::ID> scene_ids;

    void BindScene(const Scene *scene)
    {
        scene_ids.push(scene == nullptr ? Scene::ID{0} : scene->GetId());
    }

    void UnbindScene()
    {
        scene_ids.pop();
    }

    Scene::ID GetScene() const
    {
        return scene_ids.empty() ? Scene::ID{0} : scene_ids.top();
    }
};

/*
 * This class holds all shaders, descriptor sets, framebuffers etc. needed for pipeline generation (which it hands off to Instance)
 *
 */
class Engine {
public:
    enum TextureFormatDefault {
        TEXTURE_FORMAT_DEFAULT_NONE    = 0,
        TEXTURE_FORMAT_DEFAULT_COLOR   = 1,
        TEXTURE_FORMAT_DEFAULT_DEPTH   = 2,
        TEXTURE_FORMAT_DEFAULT_GBUFFER = 4,
        TEXTURE_FORMAT_DEFAULT_STORAGE = 8
    };

    Engine(SystemSDL &, const char *app_name);
    ~Engine();
    
    inline Instance *GetInstance() const { return m_instance.get(); }
    inline Device   *GetDevice() const { return m_instance ? m_instance->GetDevice() : nullptr; }

    inline DeferredRenderer &GetDeferredRenderer() { return m_deferred_renderer; }
    inline const DeferredRenderer &GetDeferredRenderer() const { return m_deferred_renderer; }

    inline RenderListContainer &GetRenderListContainer() { return m_render_list_container; }
    inline const RenderListContainer &GetRenderListContainer() const { return m_render_list_container; }

    inline Octree &GetOctree() { return m_octree; }
    inline const Octree &GetOctree() const { return m_octree; }

    inline Image::InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }

    Ref<GraphicsPipeline> FindOrCreateGraphicsPipeline(
        Ref<Shader> &&shader,
        const VertexAttributeSet &vertex_attributes,
        Bucket bucket
    );
    
    Ref<GraphicsPipeline> AddGraphicsPipeline(std::unique_ptr<GraphicsPipeline> &&pipeline)
    {
        const auto bucket = pipeline->GetBucket();

        auto graphics_pipeline = resources.graphics_pipelines.Add(std::move(pipeline));

        m_render_list_container.Get(bucket).AddGraphicsPipeline(graphics_pipeline.Acquire());

        return graphics_pipeline;
    }

    void SetSpatialTransform(Spatial *spatial, const Transform &transform);
    

    void Initialize();
    void Destroy();
    void PrepareSwapchain();
    void Compile();

    void ResetRenderBindings();
    void UpdateRendererBuffersAndDescriptors(uint32_t frame_index);

    void RenderShadows(CommandBuffer *primary, uint32_t frame_index);
    void RenderDeferred(CommandBuffer *primary, uint32_t frame_index);
    void RenderSwapchain(CommandBuffer *command_buffer) const;


    ShaderGlobals          *shader_globals;

    EngineCallbacks         callbacks;
    Resources               resources;
    Assets                  assets;
    ShaderManager           shader_manager;

    RenderBindings          render_bindings;

    std::mutex render_mutex;
    std::atomic_bool m_running{false};

private:
    void FindTextureFormatDefaults();

    std::unique_ptr<Instance>         m_instance;
    std::unique_ptr<GraphicsPipeline> m_root_pipeline;

    EnumOptions<TextureFormatDefault, Image::InternalFormat, 5> m_texture_format_defaults;

    DeferredRenderer m_deferred_renderer;
    ShadowRenderer   m_shadow_renderer;
    RenderListContainer       m_render_list_container;


    Octree::Root m_octree_root;
    Octree       m_octree;

    /* TMP */
    std::vector<std::unique_ptr<renderer::Attachment>> m_render_pass_attachments;
};

} // namespace hyperion::v2

#endif

