#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include "components/shader.h"
#include "components/framebuffer.h"
#include "components/filter_stack.h"
#include <rendering/backend/renderer_command_buffer.h>
#include "components/pipeline.h"

#include <util/enum_options.h>

#include <memory>

namespace hyperion::v2 {

using renderer::Instance;

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
        TEXTURE_FORMAT_DEFAULT_GBUFFER = 4
    };

    Engine(SystemSDL &, const char *app_name);
    ~Engine();

    inline Instance *GetInstance() { return m_instance.get(); }
    inline const Instance *GetInstance() const { return m_instance.get(); }

    inline SwapchainData &GetSwapchainData() { return m_swapchain_data; }
    inline const SwapchainData &GetSwapchainData() const { return m_swapchain_data; }

    inline FilterStack &GetFilterStack() { return m_filter_stack; }
    inline const FilterStack &GetFilterStack() const { return m_filter_stack; }

    inline Texture::TextureInternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }

    Shader::ID AddShader(std::unique_ptr<Shader> &&shader);
    inline Shader *GetShader(Shader::ID id)
        { return GetObject(m_shaders, id); }
    inline const Shader *GetShader(Shader::ID id) const
        { return const_cast<Engine*>(this)->GetShader(id); }

    Framebuffer::ID AddFramebuffer(std::unique_ptr<Framebuffer> &&framebuffer, RenderPass::ID render_pass);
    Framebuffer::ID AddFramebuffer(size_t width, size_t height, RenderPass::ID render_pass);
    inline Framebuffer *GetFramebuffer(Framebuffer::ID id)
        { return GetObject(m_framebuffers, id); }
    inline const Framebuffer *GetFramebuffer(Framebuffer::ID id) const
        { return const_cast<Engine*>(this)->GetFramebuffer(id); }

    RenderPass::ID AddRenderPass(std::unique_ptr<RenderPass> &&render_pass);
    inline RenderPass *GetRenderPass(RenderPass::ID id)
        { return GetObject(m_render_passes, id); }
    inline const RenderPass *GetRenderPass(RenderPass::ID id) const
        { return const_cast<Engine*>(this)->GetRenderPass(id); }

    /* Pipelines will be deferred until descriptor sets are built */
    Pipeline::ID AddPipeline(renderer::Pipeline::Builder &&builder);
    HYP_FORCE_INLINE Pipeline *GetPipeline(Pipeline::ID id)
        { return GetObject(m_pipelines, id); }
    HYP_FORCE_INLINE const Pipeline *GetPipeline(Pipeline::ID id) const
        { return const_cast<Engine*>(this)->GetPipeline(id); }


    void Initialize();
    void PrepareSwapchain();
    void BuildPipelines();
    void RenderPostProcessing(Frame *frame, uint32_t frame_index);
    void RenderSwapchain(Frame *frame);

private:
    void InitializeInstance();
    void FindTextureFormatDefaults();

    template <class T>
    HYP_FORCE_INLINE constexpr T *GetObject(std::vector<std::unique_ptr<T>> &objects, const typename T::ID &id)
    {
        return MathUtil::InRange(id.GetValue(), {1, objects.size() + 1})
            ? objects[id.GetValue() - 1].get()
            : nullptr;
    }

    template <class T>
    inline T::ID AddObject(std::vector<std::unique_ptr<T>> &objects, std::unique_ptr<T> &&object)
    {
        typename T::ID result{};

        /* Find an existing slot */
        auto it = std::find(objects.begin(), objects.end(), nullptr);

        if (it != objects.end()) {
            result = T::ID(it - objects.begin());
            objects[it] = std::move(object);
        } else {
            result = objects.size();
            objects.push_back(std::move(object));
        }

        return result;
    }

    template<class T>
    inline void RemoveObjectById(std::vector<std::unique_ptr<T>> &objects, T::ID id)
    {
        /* Cannot simply erase, as that would invalidate existing IDs */
        objects[id] = {};
    }

    EnumOptions<TextureFormatDefault, Texture::TextureInternalFormat, 4> m_texture_format_defaults;

    FilterStack m_filter_stack;

    std::vector<std::unique_ptr<Shader>> m_shaders;
    std::vector<std::unique_ptr<Framebuffer>> m_framebuffers;
    std::vector<std::unique_ptr<RenderPass>> m_render_passes;
    std::vector<std::unique_ptr<Pipeline>> m_pipelines;
    std::unique_ptr<Instance> m_instance;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_SHADER_H

