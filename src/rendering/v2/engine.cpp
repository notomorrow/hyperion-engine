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
      m_material_storage_buffer(nullptr),
      m_object_storage_buffer(nullptr)
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
    m_materials.RemoveAll(this);
    m_compute_pipelines.RemoveAll(this);

    m_swapchain_render_container->Destroy(this);
    m_graphics_pipelines.RemoveAll(this);

    if (m_material_storage_buffer != nullptr) {
        m_material_storage_buffer->Destroy(m_instance->GetDevice());
        delete m_material_storage_buffer;
    }

    if (m_object_storage_buffer != nullptr) {
        m_object_storage_buffer->Destroy(m_instance->GetDevice());
        delete m_object_storage_buffer;
    }

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

    Shader::ID shader_id{};

    // TODO: should be moved elsewhere. SPIR-V for rendering quad could be static
    {
        shader_id = AddShader(std::make_unique<Shader>(std::vector<SubShader>{
            {ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_vert.spv").Read()}},
            {ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_frag.spv").Read()}}
        }));
    }

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

    m_swapchain_render_container = std::make_unique<GraphicsPipeline>(shader_id, render_pass_id);

    
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

        m_swapchain_render_container->AddFramebuffer(AddFramebuffer(std::move(fbo), render_pass_id));
    }

    m_swapchain_render_container->SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);
}

void Engine::Initialize()
{
    InitializeInstance();
    FindTextureFormatDefaults();

    /* Material uniform buffer - all materials are added into
     * this buffer and will be dynamically swapped in and out
     */

    m_material_storage_buffer = new StorageBuffer();
    m_material_storage_buffer->Create(m_instance->GetDevice(), ShaderStorageData::max_materials_bytes);

    m_object_storage_buffer = new StorageBuffer();
    m_object_storage_buffer->Create(m_instance->GetDevice(), ShaderStorageData::max_objects_bytes);

    /* for materials */
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        ->AddSubDescriptor({
            .gpu_buffer = m_material_storage_buffer,
            .range = sizeof(MaterialShaderData)
        });

    /* for objects */
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        ->AddSubDescriptor({
            .gpu_buffer = m_object_storage_buffer,
            .range = sizeof(ObjectShaderData)
        });
}

void Engine::Compile()
{
    /* Finalize materials */
    m_material_storage_buffer->Copy(
        m_instance->GetDevice(),
        m_shader_storage_data.materials.ByteSize(),
        m_shader_storage_data.materials.Data()
    );

    /* Finalize per-object data */
    m_object_storage_buffer->Copy(
        m_instance->GetDevice(),
        m_shader_storage_data.objects.ByteSize(),
        m_shader_storage_data.objects.Data()
    );

    /* Finalize descriptor pool */
    auto descriptor_pool_result = m_instance->GetDescriptorPool().Create(m_instance->GetDevice());
    AssertThrowMsg(descriptor_pool_result, "%s", descriptor_pool_result.message);

    m_filter_stack.BuildPipelines(this);

    m_swapchain_render_container->Create(this);
    m_graphics_pipelines.CreateAll(this);
    m_compute_pipelines.CreateAll(this);
}

void Engine::UpdateDescriptorData()
{
    if (m_shader_storage_data.dirty_object_range_end) {
        AssertThrow(m_shader_storage_data.dirty_object_range_end > m_shader_storage_data.dirty_object_range_start);

        m_object_storage_buffer->Copy(
            m_instance->GetDevice(),
            m_shader_storage_data.dirty_object_range_start * sizeof(ObjectShaderData),
            (m_shader_storage_data.dirty_object_range_end - m_shader_storage_data.dirty_object_range_start) * sizeof(ObjectShaderData),
            m_shader_storage_data.objects.Data() + m_shader_storage_data.dirty_object_range_start
        );
    }

    m_shader_storage_data.dirty_object_range_start = 0;
    m_shader_storage_data.dirty_object_range_end = 0;
}


void Engine::RenderPostProcessing(CommandBuffer *primary_command_buffer, uint32_t frame_index)
{
    m_filter_stack.Render(this, primary_command_buffer, frame_index);
}

void Engine::RenderSwapchain(CommandBuffer *command_buffer)
{
    renderer::GraphicsPipeline &pipeline = m_swapchain_render_container->Get();

    pipeline.Bind(command_buffer);

    m_instance->GetDescriptorPool().BindDescriptorSets(command_buffer, &pipeline, 0, 3);

    Filter::full_screen_quad->RenderVk(command_buffer, m_instance.get(), nullptr);
}
} // namespace hyperion::v2