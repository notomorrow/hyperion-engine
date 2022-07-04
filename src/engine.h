#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include <asset/assets.h>

#include <rendering/post_fx.h>
#include <rendering/render_list.h>
#include <rendering/deferred.h>
#include <rendering/render_components/shadows.h>
#include <rendering/resources.h>
#include <rendering/shader_manager.h>
#include <rendering/renderable_attributes.h>
#include <rendering/default_formats.h>
#include <rendering/dummy_data.h>
#include <scene/World.h>

#include "game_thread.h"
#include "threads.h"
#include "task_thread.h"

#include <core/ecs/component_registry.h>
#include <core/scheduler.h>
#include <core/lib/flat_map.h>
#include <core/lib/type_map.h>

#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_image_view.h>
#include <rendering/backend/renderer_sampler.h>
#include <rendering/backend/renderer_semaphore.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <util/enum_options.h>
#include <builders/shader_compiler/shader_compiler.h>

#include <types.h>

#include <memory>
#include <mutex>
#include <stack>

#define HYP_FLUSH_RENDER_QUEUE(engine) \
    do { \
        (engine)->render_scheduler.FlushOrWait([](auto &fn) { \
            HYPERION_ASSERT_RESULT(fn(nullptr, 0)); \
        }); \
    } while (0)

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;
using renderer::Semaphore;
using renderer::SemaphoreChain;
using renderer::Image;
using renderer::StorageBuffer;

class Engine;

/* Current descriptor / attachment layout */

/*
 * | ====== Set 0 ====== | ====== Set 1 ====== | ====== Set 2 ====== | ====== Set 3 ====== | ====== Set 4 ====== |
 * | (UNUSED)            | GBuffer: textures[] | Scene data SSBO     | Material data SSBO  | Bindless textures   |
 * |                     | Gbuffer: depth      | empty               | Object data SSBO    | empty               |
 * |                     |                     | empty               | Skeleton data SSBO  | empty               |
 * |                     |                     | empty               | empty               | empty               |
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

using RenderableDeletionMaskBits = UInt;

enum RenderableDeletionMask : RenderableDeletionMaskBits {
    RENDERABLE_DELETION_NONE      = 0,
    RENDERABLE_DELETION_TEXTURES  = 1 << 0,
    RENDERABLE_DELETION_MATERIALS = 1 << 1,
    RENDERABLE_DELETION_MESHES    = 1 << 2,
    RENDERABLE_DELETION_SKELETONS = 1 << 3,
    RENDERABLE_DELETION_SHADERS   = 1 << 4
};

struct RenderState {
    struct SceneBinding {
        Scene::ID id;
        Scene::ID parent_id;

        explicit operator bool() const { return bool(id); }
    };

    std::stack<SceneBinding> scene_ids;
    FlatSet<Light::ID>       light_ids;

    void BindLight(Light::ID light)
    {
        light_ids.Insert(light);
    }

    void UnbindLight(Light::ID light)
    {
        light_ids.Erase(light);
    }

    void BindScene(const Scene *scene)
    {
        scene_ids.push(
            scene == nullptr
                ? SceneBinding{}
                : SceneBinding{scene->GetId(), scene->GetParentId()}
        );
    }

    void UnbindScene()
    {
        scene_ids.pop();
    }

    SceneBinding GetScene() const
    {
        return scene_ids.empty() ? SceneBinding{} : scene_ids.top();
    }
};

struct GraphicsPipelineAttributeSet {
    Ref<Shader>        shader;
    VertexAttributeSet vertex_attributes;
    Bucket             bucket;

    bool operator<(const GraphicsPipelineAttributeSet &other) const
    {
        const auto shader_id = shader ? shader->GetId() : Shader::ID{0};
        const auto other_shader_id = other.shader ? other.shader->GetId() : Shader::ID{0};

        return std::tie(shader_id, vertex_attributes, bucket) <
            std::tie(other_shader_id, other.vertex_attributes, other.bucket);
    }

    bool operator==(const GraphicsPipelineAttributeSet &other) const
    {
        return shader == other.shader
            && vertex_attributes == other.vertex_attributes
            && bucket == other.bucket;
    }
};

struct RenderFunctor : public ScheduledFunction<renderer::Result, CommandBuffer * /* command_buffer */, UInt /* frame_index */>
{
    static constexpr UInt data_buffer_size = 256;

    UByte data_buffer[data_buffer_size];

    RenderFunctor() = default;

    template <class Lambda>
    RenderFunctor(Lambda &&lambda)
        : ScheduledFunction(std::forward<Lambda>(lambda))
    {
    }

    template <class DataStruct, class Lambda>
    RenderFunctor(DataStruct &&data_struct, Lambda &&lambda)
        : ScheduledFunction(std::forward<Lambda>(lambda))
    {
        static_assert(sizeof(data_struct) <= data_buffer_size, "DataStruct does not fit into buffer!");
        static_assert(std::is_pod_v<DataStruct>, "DataStruct must be a POD object!");

        std::memcpy(&data_buffer[0], &data_struct, sizeof(data_struct));
    }
};

/*
 * This class holds all shaders, descriptor sets, framebuffers etc. needed for pipeline generation (which it hands off to Instance)
 *
 */
class Engine {
public:
    Engine(SystemSDL &, const char *app_name);
    ~Engine();
    
    Instance *GetInstance() const                                  { return m_instance.get(); }
    Device   *GetDevice() const                                    { return m_instance ? m_instance->GetDevice() : nullptr; }

    DeferredRenderer &GetDeferredRenderer()                        { return m_deferred_renderer; }
    const DeferredRenderer &GetDeferredRenderer() const            { return m_deferred_renderer; }

    RenderListContainer &GetRenderListContainer()                  { return m_render_list_container; }
    const RenderListContainer &GetRenderListContainer() const      { return m_render_list_container; }

    auto &GetRenderScheduler()                                     { return render_scheduler; }
    const auto &GetRenderScheduler() const                         { return render_scheduler; }

    auto &GetShaderData()                                          { return shader_globals; }
    const auto &GetShaderData() const                              { return shader_globals; }
    
    auto &GetDummyData()                                           { return m_dummy_data; }
    const auto &GetDummyData() const                               { return m_dummy_data; }
    
    ComponentRegistry<Spatial> &GetComponentRegistry()             { return m_component_registry; }
    const ComponentRegistry<Spatial> &GetComponentRegistry() const { return m_component_registry; }
    
    World &GetWorld()                                              { return m_world; }
    const World &GetWorld() const                                  { return m_world; }

    void SafeReleaseRenderable(Ref<Texture> &&renderable)
    {
        std::lock_guard guard(m_renderable_deletion_mutex);
        
        std::get<std::vector<RenderableDeletionEntry<Texture>>>(m_renderable_deletion_queue_items).push_back({
            .ref     = std::move(renderable),
            .deleter = [](Ref<Texture> &&ref) {
                ref.Reset();
            }
        });

        m_renderable_deletion_flag |= RENDERABLE_DELETION_TEXTURES;
    }
    
    /*void SafeReleaseRenderable(Ref<Material> &&renderable)
    {
        std::lock_guard guard(m_renderable_deletion_mutex);
        
        std::get<std::vector<RenderableDeletionEntry<Material>>>(m_renderable_deletion_queue_items).push_back({
            .ref     = std::move(renderable),
            .deleter = [](Ref<Material> &&ref) {
                ref.Reset();
            }
        });

        m_renderable_deletion_flag |= RENDERABLE_DELETION_MATERIALS;
    }*/
    
    void SafeReleaseRenderable(Ref<Mesh> &&renderable)
    {
        std::lock_guard guard(m_renderable_deletion_mutex);
        
        std::get<std::vector<RenderableDeletionEntry<Mesh>>>(m_renderable_deletion_queue_items).push_back({
            .ref     = std::move(renderable),
            .deleter = [](Ref<Mesh> &&ref) {
                ref.Reset();
            }
        });

        m_renderable_deletion_flag |= RENDERABLE_DELETION_MESHES;
    }
    
    void SafeReleaseRenderable(Ref<Skeleton> &&renderable)
    {
        std::lock_guard guard(m_renderable_deletion_mutex);
        
        std::get<std::vector<RenderableDeletionEntry<Skeleton>>>(m_renderable_deletion_queue_items).push_back({
            .ref     = std::move(renderable),
            .deleter = [](Ref<Skeleton> &&ref) {
                ref.Reset();
            }
        });

        m_renderable_deletion_flag |= RENDERABLE_DELETION_SKELETONS;
    }
    
    void SafeReleaseRenderable(Ref<Shader> &&renderable)
    {
        std::lock_guard guard(m_renderable_deletion_mutex);
        
        std::get<std::vector<RenderableDeletionEntry<Shader>>>(m_renderable_deletion_queue_items).push_back({
            .ref     = std::move(renderable),
            .deleter = [](Ref<Shader> &&ref) {
                ref.Reset();
            }
        });

        m_renderable_deletion_flag |= RENDERABLE_DELETION_SHADERS;
    }

    Image::InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }

    Ref<GraphicsPipeline> FindOrCreateGraphicsPipeline(const RenderableAttributeSet &renderable_attributes);
    Ref<GraphicsPipeline> AddGraphicsPipeline(std::unique_ptr<GraphicsPipeline> &&pipeline);

    void Initialize();
    void PrepareSwapchain();
    void Compile();

    void ResetRenderState();
    void UpdateBuffersAndDescriptors(UInt frame_index);
    
    void RenderDeferred(Frame *frame);
    void RenderFinalPass(Frame *frame) const;

    ShaderGlobals          *shader_globals;

    EngineCallbacks         callbacks;
    Resources               resources;
    Assets                  assets;
    ShaderManager           shader_manager;

    RenderState             render_state;
    
    std::atomic_bool        m_running{false};

    Scheduler<RenderFunctor> render_scheduler;

    GameThread game_thread;
    TaskThread terrain_thread;

private:
    void FindTextureFormatDefaults();
    
    std::unique_ptr<Instance>         m_instance;
    std::unique_ptr<GraphicsPipeline> m_root_pipeline;

    EnumOptions<TextureFormatDefault, Image::InternalFormat, 16> m_texture_format_defaults;

    DeferredRenderer    m_deferred_renderer;
    RenderListContainer m_render_list_container;

    /* TMP */
    std::vector<std::unique_ptr<renderer::Attachment>> m_render_pass_attachments;

    FlatMap<RenderableAttributeSet, GraphicsPipeline::ID> m_graphics_pipeline_mapping;

    ComponentRegistry<Spatial> m_component_registry;

    DummyData m_dummy_data;

    template <class T>
    struct RenderableDeletionEntry {
        static_assert(std::is_base_of_v<RenderObject, T>, "T must be a derived class of RenderObject");

        using Deleter = std::add_pointer_t<void(Ref<T> &&)>;

        UInt    cycles_remaining = max_frames_in_flight;
        Ref<T>  ref;
        Deleter deleter;
    };

    // returns true if all were completed
    template <class T>
    bool PerformEnqueuedDeletions()
    {
        auto &queue = std::get<std::vector<RenderableDeletionEntry<T>>>(m_renderable_deletion_queue_items);

        for (auto it = queue.begin(); it != queue.end();) {
            auto &front = queue.front();

            if (!--front.cycles_remaining) {
                front.deleter(std::move(front.ref));

                it = queue.erase(it);
            } else {
                ++it;
            }
        }

        return queue.empty();
    }

    std::tuple<
        std::vector<RenderableDeletionEntry<Texture>>,
        std::vector<RenderableDeletionEntry<Mesh>>,
        std::vector<RenderableDeletionEntry<Skeleton>>,
        std::vector<RenderableDeletionEntry<Shader>>
    > m_renderable_deletion_queue_items;

    std::mutex                                     m_renderable_deletion_mutex;
    std::atomic<RenderableDeletionMaskBits>        m_renderable_deletion_flag{RENDERABLE_DELETION_NONE};

    World                                          m_world;
};

} // namespace hyperion::v2

#endif

