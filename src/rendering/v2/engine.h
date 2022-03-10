#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include "components/shader.h"
#include "components/framebuffer.h"
#include <rendering/backend/renderer_pipeline.h>

#include <util/enum_options.h>

#include <memory>

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Pipeline;

/*
 * This class holds all shaders, descriptor sets, framebuffers etc. needed for pipeline generation (which it hands off to Instance)
 *
 */
class Engine {
public:
    /* Our "root" shader/pipeline -- used for rendering a quad to the screen. */
    struct SwapchainData {
        std::unique_ptr<Shader> shader;
        Pipeline *pipeline;
    } m_swapchain_data;

    enum TextureFormatDefault {
        TEXTURE_FORMAT_DEFAULT_NONE    = 0,
        TEXTURE_FORMAT_DEFAULT_COLOR   = 1,
        TEXTURE_FORMAT_DEFAULT_DEPTH   = 2,
        TEXTURE_FORMAT_DEFAULT_GBUFFER = 4
    };

    Engine(SystemSDL &, const char *app_name);
    ~Engine();

    inline Instance *GetInstance() { return m_instance.get(); }
    inline const Instance *GetInstance() const { return m_instance.get(); }

    inline SwapchainData &GetSwapchainData() { return m_swapchain_data; }
    inline const SwapchainData &GetSwapchainData() const { return m_swapchain_data; }

    inline Texture::TextureInternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }

    Shader::ID AddShader(std::unique_ptr<Shader> &&shader);
    inline Shader *GetShader(Shader::ID id)
    {
        return MathUtil::InRange(id, {0, m_shaders.size()})
            ? m_shaders[id].get()
            : nullptr;
    }

    inline const Shader *GetShader(Shader::ID id) const
        { return const_cast<Engine*>(this)->GetShader(id); }

    Framebuffer::ID AddFramebuffer(std::unique_ptr<Framebuffer> &&framebuffer, RenderPass::ID render_pass);
    Framebuffer::ID AddFramebuffer(size_t width, size_t height, RenderPass::ID render_pass);
    inline Framebuffer *GetFramebuffer(Framebuffer::ID id)
    {
        return MathUtil::InRange(id, {0, m_framebuffers.size()})
            ? m_framebuffers[id].get()
            : nullptr;
    }
    inline const Framebuffer *GetFramebuffer(Framebuffer::ID id) const
        { return const_cast<Engine*>(this)->GetFramebuffer(id); }

    RenderPass::ID AddRenderPass(std::unique_ptr<RenderPass> &&render_pass);
    inline RenderPass *GetRenderPass(RenderPass::ID id)
    {
        return MathUtil::InRange(id, {0, m_render_passes.size()})
            ? m_render_passes[id].get()
            : nullptr;
    }

    inline const RenderPass *GetRenderPass(RenderPass::ID id) const
        { return const_cast<Engine*>(this)->GetRenderPass(id); }

    void AddPipeline(Pipeline::Builder &&builder, Pipeline **out = nullptr);


    void Initialize();
    void PrepareSwapchain();

private:
    void InitializeInstance();
    void FindTextureFormatDefaults();

    EnumOptions<TextureFormatDefault, Texture::TextureInternalFormat, 4> m_texture_format_defaults;

    std::vector<std::unique_ptr<Shader>> m_shaders;
    std::vector<std::unique_ptr<Framebuffer>> m_framebuffers;
    std::vector<std::unique_ptr<RenderPass>> m_render_passes;
    std::unique_ptr<Instance> m_instance;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_SHADER_H

