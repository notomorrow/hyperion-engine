#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include "components/shader.h"
#include "components/filter_stack.h"
#include "components/framebuffer.h"
#include "components/compute.h"
#include "components/util.h"
#include "components/render_container.h"
#include "components/material.h"
#include "components/texture.h"
#include "components/render_container.h"

#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_semaphore.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <util/enum_options.h>

#include <memory>

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;
using renderer::Semaphore;
using renderer::SemaphoreChain;
using renderer::Image;

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

    inline Instance *GetInstance() { return m_instance.get(); }
    inline const Instance *GetInstance() const { return m_instance.get(); } [[nodiscard]]

    inline FilterStack &GetFilterStack() { return m_filter_stack; }
    inline const FilterStack &GetFilterStack() const { return m_filter_stack; } [[nodiscard]]

    inline Image::InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }
    
    template <class ...Args>
    Shader::ID AddShader(std::unique_ptr<Shader> &&shader, Args &&... args)
        { return m_shaders.Add(this, std::move(shader), std::move(args)...); }

    inline Shader *GetShader(Shader::ID id)
        { return m_shaders.Get(id); }

    inline const Shader *GetShader(Shader::ID id) const
        { return const_cast<Engine*>(this)->GetShader(id); }
    
    template <class ...Args>
    Texture::ID AddTexture(std::unique_ptr<Texture> &&texture, Args &&... args)
        { return m_textures.Add(this, std::move(texture), std::move(args)...); }

    inline Texture *GetTexture(Texture::ID id)
        { return m_textures.Get(id); }

    inline const Texture *GetTexture(Texture::ID id) const
        { return const_cast<Engine*>(this)->GetTexture(id); }

    /* Initialize the FBO based on the given RenderPass's attachments */
    Framebuffer::ID AddFramebuffer(std::unique_ptr<Framebuffer> &&framebuffer, RenderPass::ID render_pass);

    /* Construct and initialize a FBO based on the given RenderPass's attachments */
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

    /* Pipelines will be deferred until descriptor sets are built
     * We take in the builder object rather than a unique_ptr,
     * so that we can reuse pipelines
     */
    template <class ...Args>
    RenderContainer::ID AddRenderContainer(std::unique_ptr<RenderContainer> &&render_container, Args &&... args)
    {
        render_container->PrepareDescriptors(this);

        return m_render_containers.Add(this, std::move(render_container), std::move(args)...);
    }

    inline RenderContainer *GetRenderContainer(RenderContainer::ID id)
        { return m_render_containers.Get(id); }

    inline const RenderContainer *GetRenderContainer(RenderContainer::ID id) const
        { return const_cast<Engine*>(this)->GetRenderContainer(id); }

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

    std::unique_ptr<RenderContainer> m_swapchain_render_container;

private:
    void InitializeInstance();
    void FindTextureFormatDefaults();

    EnumOptions<TextureFormatDefault, Image::InternalFormat, 5> m_texture_format_defaults;

    FilterStack m_filter_stack;

    ObjectHolder<Shader> m_shaders;
    ObjectHolder<Texture> m_textures;
    ObjectHolder<Framebuffer> m_framebuffers;
    ObjectHolder<RenderPass> m_render_passes;
    ObjectHolder<RenderContainer> m_render_containers{.defer_create = true};
    ObjectHolder<ComputePipeline> m_compute_pipelines{.defer_create = true};
    
    std::unique_ptr<Instance> m_instance;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_SHADER_H

