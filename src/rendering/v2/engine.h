#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include "components/shader.h"
#include "components/post_fx.h"
#include "components/framebuffer.h"
#include "components/compute.h"
#include "components/util.h"
#include "components/graphics.h"
#include "components/material.h"
#include "components/texture.h"
#include "components/render_list.h"
#include "components/deferred.h"
#include "components/octree.h"

#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_semaphore.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <util/enum_options.h>

#include <memory>
#include <map>
#include <typeindex>

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;
using renderer::Semaphore;
using renderer::SemaphoreChain;
using renderer::Image;
using renderer::StorageBuffer;
/*
 * This class holds all shaders, descriptor sets, framebuffers etc. needed for pipeline generation (which it hands off to Instance)
 *
 */
class Engine {
public:

    enum EventKey {
        EVENT_KEY_ENGINE             = 0,
        EVENT_KEY_GRAPHICS_PIPELINES = 1,
        EVENT_KEY_DESCRIPTOR_SETS    = 2
    };

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
    inline const Instance *GetInstance() const { return m_instance.get(); }

    inline PostProcessing &GetPostProcessing() { return m_post_processing; }
    inline const PostProcessing &GetPostProcessing() const { return m_post_processing; }

    inline DeferredRenderer &GetDeferredRenderer() { return m_deferred_rendering; }
    inline const DeferredRenderer &GetDeferredRenderer() const { return m_deferred_rendering; }

    inline Octree &GetOctree() { return m_octree; }
    inline const Octree &GetOctree() const { return m_octree; }

    inline Image::InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.Get(type); }

    inline auto &GetEvents()
        { return m_events; }

    inline const auto &GetEvents() const
        { return m_events; }
    
    inline auto &GetEvents(EventKey key)
        { return m_events[key]; }
    
    inline const auto &GetEvents(EventKey key) const
        { return const_cast<Engine*>(this)->GetEvents(key); }

    /* Shaders */

    template <class ...Args>
    Shader::ID AddShader(std::unique_ptr<Shader> &&shader, Args &&... args)
        { return m_shaders.Add(this, std::move(shader), std::move(args)...); }

    template <class ...Args>
    void RemoveShader(Shader::ID id, Args &&... args)
        { return m_shaders.Remove(this, id, std::move(args)...); }

    inline Shader *GetShader(Shader::ID id)
        { return m_shaders.Get(id); }

    inline const Shader *GetShader(Shader::ID id) const
        { return const_cast<Engine*>(this)->GetShader(id); }

    /* Textures */

    template <class ...Args>
    Texture::ID AddTexture(std::unique_ptr<Texture> &&texture, Args &&... args)
        { return m_textures.Add(this, std::move(texture), std::move(args)...); }

    template <class ...Args>
    void RemoveTexture(Texture::ID id, Args &&... args)
        { return m_textures.Remove(this, id, std::move(args)...); }

    inline Texture *GetTexture(Texture::ID id)
        { return m_textures.Get(id); }

    inline const Texture *GetTexture(Texture::ID id) const
        { return const_cast<Engine*>(this)->GetTexture(id); }

    /* Initialize the FBO based on the given RenderPass's attachments */
    Framebuffer::ID AddFramebuffer(std::unique_ptr<Framebuffer> &&framebuffer, RenderPass::ID render_pass);

    /* Construct and initialize a FBO based on the given RenderPass's attachments */
    Framebuffer::ID AddFramebuffer(size_t width, size_t height, RenderPass::ID render_pass);

    template <class ...Args>
    void RemoveFramebuffer(Framebuffer::ID id, Args &&... args)
        { return m_framebuffers.Remove(this, id, std::move(args)...); }

    inline Framebuffer *GetFramebuffer(Framebuffer::ID id)
        { return m_framebuffers.Get(id); }

    inline const Framebuffer *GetFramebuffer(Framebuffer::ID id) const
        { return const_cast<Engine*>(this)->GetFramebuffer(id); }
    
    template <class ...Args>
    RenderPass::ID AddRenderPass(std::unique_ptr<RenderPass> &&render_pass, Args &&... args)
        { return m_render_passes.Add(this, std::move(render_pass), std::move(args)...); }

    template <class ...Args>
    void RemoveRenderPass(RenderPass::ID id, Args &&... args)
        { return m_render_passes.Remove(this, id, std::move(args)...); }

    inline RenderPass *GetRenderPass(RenderPass::ID id)
        { return m_render_passes.Get(id); }

    inline const RenderPass *GetRenderPass(RenderPass::ID id) const
        { return const_cast<Engine*>(this)->GetRenderPass(id); }

    /* Materials - will all be jammed into a shader storage buffer object */
    template <class ...Args>
    Material::ID AddMaterial(std::unique_ptr<Material> &&material, Args &&... args)
    {
        MaterialShaderData material_shader_data;

        const Material::ID id = m_materials.Add(this, std::move(material), &material_shader_data, std::move(args)...);

        m_shader_globals->materials.Set(id.value - 1, std::move(material_shader_data));

        return id;
    }

    template <class ...Args>
    void RemoveMaterial(Material::ID id, Args &&... args)
        { return m_materials.Remove(this, id, std::move(args)...); }

    inline Material *GetMaterial(Material::ID id)
        { return m_materials.Get(id); }

    inline const Material *GetMaterial(Material::ID id) const
        { return const_cast<Engine*>(this)->GetMaterial(id); }

    /* Pipelines will be deferred until descriptor sets are built
     * We take in the builder object rather than a unique_ptr,
     * so that we can reuse pipelines
     */
    template <class ...Args>
    GraphicsPipeline::ID AddGraphicsPipeline(std::unique_ptr<GraphicsPipeline> &&pipeline, Args &&... args)
    {
        const auto bucket = pipeline->GetBucket();

        GraphicsPipeline::ID id = m_deferred_rendering.GetRenderList()[bucket]
            .pipelines.Add(this, std::move(pipeline), std::move(args)...);

        id.bucket = bucket;

        return id;
    }

    template <class ...Args>
    void RemoveGraphicsPipeline(GraphicsPipeline::ID id, Args &&... args)
        { return m_deferred_rendering.GetRenderList()[id.bucket].pipelines.Remove(this, id, std::move(args)...); }

    inline GraphicsPipeline *GetGraphicsPipeline(GraphicsPipeline::ID id)
        { return m_deferred_rendering.GetRenderList()[id.bucket].pipelines.Get(id); }

    inline const GraphicsPipeline *GetGraphicsPipeline(GraphicsPipeline::ID id) const
        { return const_cast<Engine*>(this)->GetGraphicsPipeline(id); }

    /* Pipelines will be deferred until descriptor sets are built */
    template <class ...Args>
    ComputePipeline::ID AddComputePipeline(std::unique_ptr<ComputePipeline> &&compute_pipeline, Args &&... args)
        { return m_compute_pipelines.Add(this, std::move(compute_pipeline), std::move(args)...); }

    template <class ...Args>
    void RemoveComputePipeline(ComputePipeline::ID id, Args &&... args)
        { return m_compute_pipelines.Remove(this, id, std::move(args)...); }

    inline ComputePipeline *GetComputePipeline(ComputePipeline::ID id)
        { return m_compute_pipelines.Get(id); }

    inline const ComputePipeline *GetComputePipeline(ComputePipeline::ID id) const
        { return const_cast<Engine*>(this)->GetComputePipeline(id); }

    /* Spatials */

    template <class ...Args>
    Spatial::ID AddSpatial(std::unique_ptr<Spatial> &&spatial, Args &&... args)
        { return m_spatials.Add(this, std::move(spatial), std::move(args)...); }

    template <class ...Args>
    void RemoveSpatial(Spatial::ID id, Args &&... args)
        { return m_spatials.Remove(this, id, std::move(args)...); }

    inline Spatial *GetSpatial(Spatial::ID id)
        { return m_spatials.Get(id); }

    inline const Spatial *GetSpatial(Spatial::ID id) const
        { return const_cast<Engine*>(this)->GetSpatial(id); }

    void SetSpatialTransform(Spatial::ID id, const Transform &transform);

    void Initialize();
    void Destroy();
    void PrepareSwapchain();
    void Compile();
    void UpdateDescriptorData(uint32_t frame_index);
    void RenderDeferred(CommandBuffer *primary, uint32_t frame_index);
    void RenderPostProcessing(CommandBuffer *primary, uint32_t frame_index);
    void RenderSwapchain(CommandBuffer *command_buffer) const;

    std::unique_ptr<GraphicsPipeline> m_swapchain_pipeline;


    ShaderGlobals *m_shader_globals;

private:
    void FindTextureFormatDefaults();

    std::unique_ptr<Instance> m_instance;

    EnumOptions<TextureFormatDefault, Image::InternalFormat, 5> m_texture_format_defaults;

    PostProcessing m_post_processing;
    DeferredRenderer m_deferred_rendering;

    Octree::Root m_octree_root;
    Octree m_octree;

    ObjectHolder<Shader> m_shaders;
    ObjectHolder<Texture> m_textures;
    ObjectHolder<Framebuffer> m_framebuffers;
    ObjectHolder<RenderPass> m_render_passes;
    ObjectHolder<Material> m_materials;
    ObjectHolder<Spatial> m_spatials;
    ObjectHolder<ComputePipeline> m_compute_pipelines{.defer_create = true};

    EnumOptions<EventKey, ComponentEvents<EngineCallbacks>, 3> m_events;
};

} // namespace hyperion::v2

#endif

