#include "engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

#include "components/filter.h"
#include "components/compute.h"

#include <rendering/backend/renderer_features.h>

#include "rendering/mesh.h"

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;
using renderer::AttachmentBase;
using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;

Engine::Engine(SystemSDL &_system, const char *app_name)
    : m_instance(new Instance(_system, app_name, "HyperionEngine")),
      m_swapchain_data{}
{
}

Engine::~Engine()
{
    (void)m_instance->GetDevice()->Wait();

    m_filter_stack.Destroy(this);

    m_framebuffers.RemoveAll(this);
    m_render_passes.RemoveAll(this);
    m_shaders.RemoveAll(this);
    m_textures.RemoveAll(this);
    m_pipelines.RemoveAll(this);
    m_compute_pipelines.RemoveAll(this);

    m_instance->Destroy();
}

Framebuffer::ID Engine::AddFramebuffer(std::unique_ptr<Framebuffer> &&framebuffer, RenderPass::ID render_pass_id)
{
    AssertThrow(framebuffer != nullptr);

    RenderPass *render_pass = GetRenderPass(render_pass_id);
    AssertThrow(render_pass != nullptr);

    return m_framebuffers.Add(this, std::move(framebuffer), &render_pass->Get());
}

Framebuffer::ID Engine::AddFramebuffer(size_t width, size_t height, RenderPass::ID render_pass_id)
{
    RenderPass *render_pass = GetRenderPass(render_pass_id);
    AssertThrow(render_pass != nullptr);

    auto framebuffer = std::make_unique<Framebuffer>(width, height);

    /* Add all attachments from the renderpass */
    for (auto &it : render_pass->Get().GetAttachments()) {
        framebuffer->Get().AddAttachment(it.format);
    }

    return AddFramebuffer(std::move(framebuffer), render_pass_id);
}

GraphicsPipeline::ID Engine::AddGraphicsPipeline(renderer::GraphicsPipeline::Builder &&builder)
{
    auto *shader = GetShader(Shader::ID{builder.m_construction_info.shader_id});

    AssertThrow(shader != nullptr);

    builder.m_construction_info.shader = non_owning_ptr(&shader->Get());

    auto *render_pass = GetRenderPass(RenderPass::ID{builder.m_construction_info.render_pass_id});
    AssertThrow(render_pass != nullptr);
    // TODO: Assert that render_pass matches the layout of what the fbo was set up with

    builder.m_construction_info.render_pass = non_owning_ptr(&render_pass->Get());

    for (auto fbo_id : builder.m_construction_info.fbo_ids) {
        if (auto fbo = GetFramebuffer(Framebuffer::ID{fbo_id})) {
            builder.m_construction_info.fbos.push_back(non_owning_ptr(&fbo->Get()));
        }
    }

    /* Cache pipeline objects */
    const HashCode::Value_t hash_code = builder.GetHashCode().Value();

    const auto it = std::find_if(m_pipelines.objects.begin(), m_pipelines.objects.end(), [hash_code](const auto &pl) {
        return pl->Get().GetConstructionInfo().GetHashCode().Value() == hash_code;
    });

    if (it != m_pipelines.objects.end()) {
        /* Return the index of the existing pipeline + 1,
         * as ID objects are one more than their index in the array. */
        const auto reused_id = GraphicsPipeline::ID{GraphicsPipeline::ID::InnerType_t((it - m_pipelines.objects.begin()) + 1)};

        DebugLog(LogType::Debug, "Re-use graphics pipeline %d\n", reused_id.GetValue());

        return reused_id;
    }
    
    /* Create the wrapper object around pipeline */
    m_pipelines.objects.push_back(std::make_unique<GraphicsPipeline>(std::move(builder.m_construction_info)));

    return GraphicsPipeline::ID{GraphicsPipeline::ID::InnerType_t(m_pipelines.objects.size())};
}

void Engine::InitializeInstance()
{
    auto renderer_initialize_result = m_instance->Initialize(true);
    AssertThrowMsg(renderer_initialize_result, "%s", renderer_initialize_result.message);
}

void Engine::FindTextureFormatDefaults()
{
    const Device *device = m_instance->GetDevice();

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_COLOR,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_STORAGE,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
        )
    );
}

void Engine::PrepareSwapchain()
{
    m_filter_stack.Create(this);

    // TODO: should be moved elsewhere. SPIR-V for rendering quad could be static
    {
        m_swapchain_data.shader_id = AddShader(std::make_unique<Shader>(std::vector{
            SpirvObject{SpirvObject::Type::VERTEX, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_vert.spv").Read()},
            SpirvObject{SpirvObject::Type::FRAGMENT, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_frag.spv").Read()}
        }));
    }
    

    // TMP trying to update a descriptor set right on 
    const auto vertex_attributes = MeshInputAttributeSet(
        MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT);

    RenderPass::ID render_pass_id{};

    {
        auto render_pass = std::make_unique<RenderPass>(renderer::RenderPass::RENDER_PASS_STAGE_PRESENT, renderer::RenderPass::RENDER_PASS_INLINE);
        /* For our color attachment */
        render_pass->Get().AddColorAttachment(
            std::make_unique<Attachment<VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR>>
                (0, m_instance->swapchain->image_format));

        render_pass->Get().AddAttachment({
            .format = m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH)
        });
        
        render_pass_id = AddRenderPass(std::move(render_pass));
    }

    renderer::GraphicsPipeline::Builder builder;

    builder
        .Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) /* full screen quad is a triangle fan */
        .Shader<Shader>(m_swapchain_data.shader_id)
        .VertexAttributes(vertex_attributes)
        .RenderPass<RenderPass>(render_pass_id);

    for (auto img : m_instance->swapchain->images) {
        auto image_view = std::make_unique<ImageView>();

        /* Create imageview independent of a Image */
        auto image_view_result = image_view->Create(
            m_instance->GetDevice(),
            img,
            m_instance->swapchain->image_format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_VIEW_TYPE_2D
        );

        AssertThrowMsg(image_view_result, "%s", image_view_result.message);

        auto fbo = std::make_unique<Framebuffer>(m_instance->swapchain->extent.width, m_instance->swapchain->extent.height);
        fbo->Get().AddAttachment(
            FramebufferObject::AttachmentImageInfo{
                .image = nullptr,
                .image_view = std::move(image_view),
                .sampler = nullptr,
                .image_needs_creation = false,
                .image_view_needs_creation = false,
                .sampler_needs_creation = true
            },
            m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_COLOR) // unused but will tell the fbo that it is not a depth texture
        );

        /* Now we add a depth buffer */
        auto result = fbo->Get().AddAttachment(m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH));
        AssertThrowMsg(result, "%s", result.message);

        builder.Framebuffer<Framebuffer>(AddFramebuffer(std::move(fbo), render_pass_id));
    }

    m_swapchain_data.pipeline_id = AddGraphicsPipeline(std::move(builder));
}

void Engine::BuildPipelines()
{
    /* Finalize descriptor pool */
    auto descriptor_pool_result = m_instance->GetDescriptorPool().Create(m_instance->GetDevice());
    AssertThrowMsg(descriptor_pool_result, "%s", descriptor_pool_result.message);

    m_filter_stack.BuildPipelines(this);

    m_pipelines.CreateAll(this, &m_instance->GetDescriptorPool());
    m_compute_pipelines.CreateAll(this);
}

void Engine::Initialize()
{
    InitializeInstance();
    FindTextureFormatDefaults();
}

void Engine::RenderPostProcessing(CommandBuffer *primary_command_buffer, uint32_t frame_index)
{
    m_filter_stack.Render(this, primary_command_buffer, frame_index);
}

void Engine::RenderSwapchain(CommandBuffer *command_buffer)
{
    GraphicsPipeline *pipeline = GetGraphicsPipeline(m_swapchain_data.pipeline_id);

    pipeline->Get().Bind(command_buffer);

    m_instance->GetDescriptorPool().BindDescriptorSets(command_buffer, &pipeline->Get());

    Filter::full_screen_quad->RenderVk(command_buffer, m_instance.get(), nullptr);
}
} // namespace hyperion::v2