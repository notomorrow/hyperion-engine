#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include "components/shader.h"
#include "components/filter_stack.h"
#include "components/framebuffer.h"
#include "components/pipeline.h"
#include "components/compute.h"
#include "components/util.h"

#include <rendering/backend/renderer_semaphore.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <util/enum_options.h>

#include <memory>

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

    inline Shader *GetShader(Shader::ID id)
        { return m_shaders.Get(id); }

    inline const Shader *GetShader(Shader::ID id) const
        { return const_cast<Engine*>(this)->GetShader(id); }

    Framebuffer::ID AddFramebuffer(std::unique_ptr<Framebuffer> &&framebuffer, RenderPass::ID render_pass);
    Framebuffer::ID AddFramebuffer(size_t width, size_t height, RenderPass::ID render_pass);

    inline Framebuffer *GetFramebuffer(Framebuffer::ID id)
        { return m_framebuffers.Get(id); }

    inline const Framebuffer *GetFramebuffer(Framebuffer::ID id) const
        { return const_cast<Engine*>(this)->GetFramebuffer(id); }
    
    template <class ...Args>
    RenderPass::ID AddRenderPass(std::unique_ptr<RenderPass> &&render_pass, Args &&... args)
        { return m_render_passes.Add(this, std::move(render_pass), std::move(args)...); }

    inline RenderPass *GetRenderPass(RenderPass::ID id)
        { return m_render_passes.Get(id); }

    inline const RenderPass *GetRenderPass(RenderPass::ID id) const
        { return const_cast<Engine*>(this)->GetRenderPass(id); }

    /* Pipelines will be deferred until descriptor sets are built */
    Pipeline::ID AddPipeline(renderer::GraphicsPipeline::Builder &&builder);

    inline Pipeline *GetPipeline(Pipeline::ID id)
        { return m_pipelines.Get(id); }

    inline const Pipeline *GetPipeline(Pipeline::ID id) const
        { return const_cast<Engine*>(this)->GetPipeline(id); }

    /* Pipelines will be deferred until descriptor sets are built */
    template <class ...Args>
    ComputePipeline::ID AddComputePipeline(std::unique_ptr<ComputePipeline> &&compute_pipeline, Args &&... args)
        { return m_compute_pipelines.Add(this, std::move(compute_pipeline), std::move(args)...); }

    inline ComputePipeline *GetComputePipeline(ComputePipeline::ID id)
        { return m_compute_pipelines.Get(id); }

    inline const ComputePipeline *GetComputePipeline(ComputePipeline::ID id) const
        { return const_cast<Engine*>(this)->GetComputePipeline(id); }

    void Initialize();
    void PrepareSwapchain();
    void BuildPipelines();
    void RenderPostProcessing(CommandBuffer *primary_command_buffer, uint32_t frame_index);
    void RenderSwapchain(CommandBuffer *command_buffer);

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

