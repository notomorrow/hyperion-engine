#include "engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

#include <rendering/backend/renderer_helpers.h>

#include "rendering/mesh.h"

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;
using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;

Engine::Engine(SystemSDL &_system, const char *app_name)
    : m_instance(new Instance(_system, app_name, "HyperionEngine"))
{
}

Engine::~Engine()
{
    // TODO: refactor
    m_swapchain_data.shader->Destroy(m_instance.get());

    for (auto &it : m_framebuffers) {
        it->Destroy(m_instance.get());
    }

    for (auto &it : m_render_passes) {
        it->Destroy(m_instance.get());
    }

    /*for (auto &shader : m_shaders) {
        shader->Destroy(m_instance.get(), m_instance->GetDevice());
    }*/

    m_instance->Destroy();
}

Shader::ID Engine::AddShader(std::unique_ptr<Shader> &&shader)
{
    AssertThrow(shader != nullptr);

    Shader::ID id(m_shaders.size());

    m_shaders.push_back(std::move(shader));

    return id;
}

Framebuffer::ID Engine::AddFramebuffer(std::unique_ptr<Framebuffer> &&framebuffer, RenderPass::ID render_pass)
{
    AssertThrow(framebuffer != nullptr);

    Framebuffer::ID id(m_framebuffers.size());
    framebuffer->Create(m_instance.get(), GetRenderPass(render_pass));

    m_framebuffers.push_back(std::move(framebuffer));

    return id;
}

RenderPass::ID Engine::AddRenderPass(std::unique_ptr<RenderPass> &&render_pass)
{
    AssertThrow(render_pass != nullptr);

    RenderPass::ID id(m_render_passes.size());
    render_pass->Create(m_instance.get());

    m_render_passes.push_back(std::move(render_pass));

    return id;
}

void Engine::AddPipeline(Pipeline::Builder &&builder, Pipeline **out)
{
    auto *render_pass = GetRenderPass(builder.m_construction_info.render_pass_id);

    AssertThrow(render_pass != nullptr);
    // TODO: Assert that render_pass matches the layout of what the fbo was set up with

    builder.m_construction_info.render_pass = non_owning_ptr(render_pass->GetWrappedObject());

    for (int fbo_id : builder.m_construction_info.fbo_ids) {
        if (auto fbo = GetFramebuffer(fbo_id)) {
            builder.m_construction_info.fbos.push_back(non_owning_ptr(fbo->GetWrappedObject()));
        }
    }

    auto add_pipeline_result = m_instance->AddPipeline(std::move(builder), out);
    AssertThrowMsg(add_pipeline_result, "%s", add_pipeline_result.message);
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
            std::array{ Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
                        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16,
                        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
                        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );
}


/*void Engine::BuildShaders()
{
    for (auto &shader : m_shaders) {
        shader->Create(m_instance.get(), m_instance->GetDevice());
    }
}*/

void Engine::PrepareSwapchain()
{
    // TODO: should be moved elsewhere. SPIR-V for rendering quad could be static
    m_swapchain_data.shader.reset(new Shader({
        SpirvObject{ SpirvObject::Type::VERTEX, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/deferred_vert.spv").Read() },
        SpirvObject{ SpirvObject::Type::FRAGMENT, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/deferred_frag.spv").Read() }
    }));

    m_swapchain_data.shader->Create(m_instance.get());

    const auto vertex_attributes = MeshInputAttributeSet(
        MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
        | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT);


    auto render_pass = std::make_unique<RenderPass>();
    /* For our color attachment */
    render_pass->GetWrappedObject()->AddAttachment(renderer::RenderPass::AttachmentInfo{
        .attachment = std::make_unique<Attachment>(
            m_instance->swapchain->image_format, /* use swapchain image format */
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            0,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        ),
        .is_depth_attachment = false
    });

    /* For our depth attachment */
    render_pass->GetWrappedObject()->AddAttachment(renderer::RenderPass::AttachmentInfo{
        .attachment = std::make_unique<Attachment>(
            renderer::helpers::ToVkFormat(m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH)),
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            1,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ),
        .is_depth_attachment = true
    });

    render_pass->GetWrappedObject()->AddDependency(VkSubpassDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    });

    RenderPass::ID render_pass_id = AddRenderPass(std::move(render_pass));

    Pipeline::Builder builder;
    builder
        .Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) /* full screen quad is a triangle fan */
        .Shader(m_swapchain_data.shader->GetWrappedObject())
        .VertexAttributes(vertex_attributes)
        .RenderPass(render_pass_id);

    for (auto img : m_instance->swapchain->images) {
        auto image_view = std::make_unique<ImageView>(VK_IMAGE_ASPECT_COLOR_BIT);

        /* Create imageview independent of a Image */
        auto image_view_result = image_view->Create(
            m_instance->GetDevice(),
            img,
            m_instance->swapchain->image_format,
            VK_IMAGE_VIEW_TYPE_2D
        );

        AssertThrowMsg(image_view_result, "%s", image_view_result.message);

        auto fbo = std::make_unique<Framebuffer>(m_instance->swapchain->extent.width, m_instance->swapchain->extent.height);
        fbo->GetWrappedObject()->AddAttachment(
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
        auto result = fbo->GetWrappedObject()->AddAttachment(m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH));
        AssertThrowMsg(result, "%s", result.message);

        builder.Framebuffer(AddFramebuffer(std::move(fbo), render_pass_id));
    }

    AddPipeline(std::move(builder), &m_swapchain_data.pipeline);
}

void Engine::Initialize()
{
    InitializeInstance();
    FindTextureFormatDefaults();
}
} // namespace hyperion::v2