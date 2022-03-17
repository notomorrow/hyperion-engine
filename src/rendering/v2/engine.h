#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include "components/shader.h"
#include "components/filter_stack.h"
#include "components/framebuffer.h"
#include "components/pipeline.h"
#include "components/compute.h"

#include <rendering/backend/renderer_semaphore.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <util/enum_options.h>

#include <memory>

#define HYPERION_V2_ADD_OBJECT_SPARSE 0

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;
using renderer::Semaphore;
using renderer::SemaphoreChain;

/*
 * This class holds all shaders, descriptor sets, framebuffers etc. needed for pipeline generation (which it hands off to Instance)
 *
 */
class Engine {
public:
    
    template <class T>
    struct ObjectHolder {
        bool defer_create = false;
        std::vector<std::unique_ptr<T>> objects;

        HYP_FORCE_INLINE constexpr
        T *Get(const typename T::ID &id)
        {
            return MathUtil::InRange(id.GetValue(), { 1, objects.size() + 1 })
                ? objects[id.GetValue() - 1].get()
                : nullptr;
        }

        HYP_FORCE_INLINE constexpr
        const T *Get(const typename T::ID &id) const
            { return const_cast<ObjectHolder<T> *>(this)->Get(id); }

        template <class ...Args>
        auto Add(Engine *engine, std::unique_ptr<T> &&object, Args &&... args) -> typename T::ID
        {
            typename T::ID result{};

            if (!defer_create) {
                object->Create(engine, std::move(args)...);
            }

#if HYPERION_V2_ADD_OBJECT_SPARSE
            /* Find an existing slot */
            auto it = std::find(objects.begin(), objects.end(), nullptr);

            if (it != objects.end()) {
                result = T::ID(it - objects.begin() + 1);
                objects[it] = std::move(object);

                return result;
            }
#endif

            objects.push_back(std::move(object));

            return typename T::ID{typename T::ID::InnerType_t(objects.size())};
        }

        template<class ...Args>
        HYP_FORCE_INLINE
        void Remove(Engine *engine, T::ID id, Args &&... args)
        {
            auto &object = objects[id];

            AssertThrowMsg(object != nullptr, "Failed to remove object with id %d -- object was nullptr.", id.GetValue());

            object->Destroy(engine, std::move(args)...);

            /* Cannot simply erase from vector, as that would invalidate existing IDs */
            objects[id].reset();
        }
        
        template<class ...Args>
        void RemoveAll(Engine *engine, Args &&...args)
        {
            for (auto &object : objects) {
                object->Destroy(engine, args...);
                object.reset();
            }
        }

        template <class ...Args>
        void CreateAll(Engine *engine, Args &&... args)
        {
            AssertThrowMsg(defer_create, "Expected defer_create to be true, "
                "otherwise objects are automatically have Create() called when added.");
            
            for (auto &object : objects) {
                object->Create(engine, args...);
            }
        }
    };

    /* Our "root" shader/pipeline -- used for rendering a quad to the screen. */
    struct SwapchainData {
        Shader::ID shader_id;
        Pipeline::ID pipeline_id;
    } m_swapchain_data;

    enum TextureFormatDefault {
        TEXTURE_FORMAT_DEFAULT_NONE    = 0,
        TEXTURE_FORMAT_DEFAULT_COLOR   = 1,
        TEXTURE_FORMAT_DEFAULT_DEPTH   = 2,
        TEXTURE_FORMAT_DEFAULT_GBUFFER = 4,
        TEXTURE_FORMAT_DEFAULT_STORAGE = 8
    };

    Engine(SystemSDL &, const char *app_name);
    ~Engine();

    inline Instance *GetInstance() { return m_instance.get(); }
    inline const Instance *GetInstance() const { return m_instance.get(); } [[nodiscard]]

    inline SwapchainData &GetSwapchainData() { return m_swapchain_data; }
    inline const SwapchainData &GetSwapchainData() const { return m_swapchain_data; } [[nodiscard]]

    inline FilterStack &GetFilterStack() { return m_filter_stack; }
    inline const FilterStack &GetFilterStack() const { return m_filter_stack; } [[nodiscard]]

    inline Texture::TextureInternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }
    
    template <class ...Args>
    Shader::ID AddShader(std::unique_ptr<Shader> &&shader, Args &&... args)
        { return m_shaders.Add(this, std::move(shader), std::move(args)...); }
    HYP_FORCE_INLINE Shader *GetShader(Shader::ID id)
        { return m_shaders.Get(id); }
    HYP_FORCE_INLINE const Shader *GetShader(Shader::ID id) const
        { return const_cast<Engine*>(this)->GetShader(id); }

    Framebuffer::ID AddFramebuffer(std::unique_ptr<Framebuffer> &&framebuffer, RenderPass::ID render_pass);
    Framebuffer::ID AddFramebuffer(size_t width, size_t height, RenderPass::ID render_pass);
    HYP_FORCE_INLINE Framebuffer *GetFramebuffer(Framebuffer::ID id)
        { return m_framebuffers.Get(id); }
    HYP_FORCE_INLINE const Framebuffer *GetFramebuffer(Framebuffer::ID id) const
        { return const_cast<Engine*>(this)->GetFramebuffer(id); }
    
    template <class ...Args>
    RenderPass::ID AddRenderPass(std::unique_ptr<RenderPass> &&render_pass, Args &&... args)
        { return m_render_passes.Add(this, std::move(render_pass), std::move(args)...); }
    HYP_FORCE_INLINE RenderPass *GetRenderPass(RenderPass::ID id)
        { return m_render_passes.Get(id); }
    HYP_FORCE_INLINE const RenderPass *GetRenderPass(RenderPass::ID id) const
        { return const_cast<Engine*>(this)->GetRenderPass(id); }

    /* Pipelines will be deferred until descriptor sets are built */
    Pipeline::ID AddPipeline(renderer::GraphicsPipeline::Builder &&builder);
    HYP_FORCE_INLINE Pipeline *GetPipeline(Pipeline::ID id)
        { return m_pipelines.Get(id); }
    HYP_FORCE_INLINE const Pipeline *GetPipeline(Pipeline::ID id) const
        { return const_cast<Engine*>(this)->GetPipeline(id); }

    /* Pipelines will be deferred until descriptor sets are built */
    template <class ...Args>
    ComputePipeline::ID AddComputePipeline(std::unique_ptr<ComputePipeline> &&compute_pipeline, Args &&... args)
        { return m_compute_pipelines.Add(this, std::move(compute_pipeline), std::move(args)...); }
    HYP_FORCE_INLINE ComputePipeline *GetComputePipeline(ComputePipeline::ID id)
        { return m_compute_pipelines.Get(id); }
    HYP_FORCE_INLINE const ComputePipeline *GetComputePipeline(ComputePipeline::ID id) const
        { return const_cast<Engine*>(this)->GetComputePipeline(id); }

    void Initialize();
    void PrepareSwapchain();
    void BuildPipelines();
    void RenderPostProcessing(CommandBuffer *primary_command_buffer, uint32_t frame_index);
    void RenderSwapchain(Frame *frame);

private:
    void InitializeInstance();
    void FindTextureFormatDefaults();

    EnumOptions<TextureFormatDefault, Texture::TextureInternalFormat, 5> m_texture_format_defaults;

    FilterStack m_filter_stack;

    ObjectHolder<Shader> m_shaders;
    ObjectHolder<Framebuffer> m_framebuffers;
    ObjectHolder<RenderPass> m_render_passes;
    ObjectHolder<Pipeline> m_pipelines{.defer_create = true};
    ObjectHolder<ComputePipeline> m_compute_pipelines{.defer_create = true};
    
    std::unique_ptr<Instance> m_instance;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_SHADER_H

