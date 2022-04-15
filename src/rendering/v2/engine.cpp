#include "engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

#include "components/post_fx.h"
#include "components/compute.h"

#include <rendering/backend/renderer_features.h>

#include <rendering/camera/ortho_camera.h>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;
using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;

Engine::Engine(SystemSDL &_system, const char *app_name)
    : m_instance(new Instance(_system, app_name, "HyperionEngine")),
      shader_globals(nullptr),
      m_octree(BoundingBox(Vector3(-250.0f), Vector3(250.0f))),
      resources(this),
      assets(this),
      m_shadow_renderer(std::make_unique<OrthoCamera>(-50, 50, -50, 50, -50, 50))
{
    m_octree.m_root = &m_octree_root;
}

Engine::~Engine()
{
    AssertThrowMsg(m_instance == nullptr, "Instance should have been destroyed");
}

void Engine::SetSpatialTransform(Spatial *spatial, const Transform &transform)
{
    AssertThrow(spatial != nullptr);
    
    spatial->UpdateShaderData(this);
}

void Engine::FindTextureFormatDefaults()
{
    const Device *device = m_instance->GetDevice();

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_COLOR,
        device->GetFeatures().FindSupportedFormat(
            std::array{ 
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_24,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
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
    m_post_processing.Create(this);
    m_deferred_renderer.Create(this);
    m_shadow_renderer.Create(this);

    auto shader = resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            {ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_vert.spv").Read()}},
            {ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_frag.spv").Read()}}
        }
    ));

    shader->Init(this);

    uint32_t iteration = 0;
    
    auto render_pass = resources.render_passes.Add(std::make_unique<RenderPass>(
        renderer::RenderPassStage::PRESENT,
        renderer::RenderPass::Mode::RENDER_PASS_INLINE
    ));

    RenderPass::ID render_pass_id{};


    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            m_instance->swapchain->extent,
            m_instance->swapchain->image_format,
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            m_instance->swapchain->extent,
            m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));
    
    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(m_instance->GetDevice()));
    }

    for (VkImage img : m_instance->swapchain->images) {
        auto fbo = std::make_unique<Framebuffer>(
            m_instance->swapchain->extent,
            render_pass.Acquire()
        );

        renderer::AttachmentRef *attachment_ref[2];

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[0]->AddAttachmentRef(
            m_instance->GetDevice(),
            img,
            renderer::Image::ToVkFormat(m_instance->swapchain->image_format),
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D,
            1, 1,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_ref[0]
        ));

        attachment_ref[0]->SetBinding(0);

        fbo->Get().AddRenderPassAttachmentRef(attachment_ref[0]);

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[1]->AddAttachmentRef(
            m_instance->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_ref[1]
        ));

        fbo->Get().AddRenderPassAttachmentRef(attachment_ref[1]);

        attachment_ref[1]->SetBinding(1);

        if (iteration == 0) {
            render_pass->Get().AddRenderPassAttachmentRef(attachment_ref[0]);
            render_pass->Get().AddRenderPassAttachmentRef(attachment_ref[1]);

            render_pass.Init();

            m_root_pipeline = std::make_unique<GraphicsPipeline>(
                shader.Acquire(),
                nullptr,
                render_pass.Acquire(),
                GraphicsPipeline::Bucket::BUCKET_SWAPCHAIN
            );
        }

        m_root_pipeline->AddFramebuffer(resources.framebuffers.Add(std::move(fbo)));

        ++iteration;
    }

    m_root_pipeline->SetTopology(Topology::TRIANGLE_FAN);

    callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        m_render_list.CreatePipelines(this);
        m_root_pipeline->Create(this);
    });

    callbacks.Once(EngineCallback::DESTROY_GRAPHICS_PIPELINES, [this](...) {
        m_render_list.Destroy(this);
        m_root_pipeline->Destroy(this);

        for (auto &attachment : m_render_pass_attachments) {
            HYPERION_ASSERT_RESULT(attachment->Destroy(m_instance->GetDevice()));
        }
    });
    
    callbacks.Once(EngineCallback::CREATE_COMPUTE_PIPELINES, [this](...) {
        resources.compute_pipelines.CreateAll(this);
    });

    callbacks.Once(EngineCallback::DESTROY_COMPUTE_PIPELINES, [this](...) {
        resources.compute_pipelines.RemoveAll(this);
    });
}

void Engine::Initialize()
{
    HYPERION_ASSERT_RESULT(m_instance->Initialize(true));

    FindTextureFormatDefaults();

    shader_globals = new ShaderGlobals(m_instance->GetFrameHandler()->NumFrames());
    
    /* for scene data */
    shader_globals->scenes.Create(m_instance->GetDevice());
    /* for materials */
    shader_globals->materials.Create(m_instance->GetDevice());
    /* for objects */
    shader_globals->objects.Create(m_instance->GetDevice());
    /* for skeletons */
    shader_globals->skeletons.Create(m_instance->GetDevice());



    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(0)
        ->AddSubDescriptor({
            .gpu_buffer = shader_globals->scenes.GetBuffers()[0].get(),
            .range = sizeof(SceneShaderData)
        });
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->AddSubDescriptor({
            .gpu_buffer = shader_globals->materials.GetBuffers()[0].get(),
            .range = sizeof(MaterialShaderData)
        });


    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->AddSubDescriptor({
            .gpu_buffer = shader_globals->objects.GetBuffers()[0].get(),
            .range = sizeof(ObjectShaderData)
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(2)
        ->AddSubDescriptor({
            .gpu_buffer = shader_globals->skeletons.GetBuffers()[0].get(),
            .range = sizeof(SkeletonShaderData)
        });



    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicUniformBufferDescriptor>(0)
        ->AddSubDescriptor({
            .gpu_buffer = shader_globals->scenes.GetBuffers()[1].get(),
            .range = sizeof(SceneShaderData)
        });
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->AddSubDescriptor({
            .gpu_buffer = shader_globals->materials.GetBuffers()[1].get(),
            .range = sizeof(MaterialShaderData)
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->AddSubDescriptor({
            .gpu_buffer = shader_globals->objects.GetBuffers()[1].get(),
            .range = sizeof(ObjectShaderData)
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(2)
        ->AddSubDescriptor({
            .gpu_buffer = shader_globals->skeletons.GetBuffers()[1].get(),
            .range = sizeof(SkeletonShaderData)
        });

    m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS)
        ->AddDescriptor<renderer::ImageSamplerDescriptor>(0);

    m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1)
        ->AddDescriptor<renderer::ImageSamplerDescriptor>(0);

    /* for textures */
    shader_globals->textures.Create(this);
    
    callbacks.TriggerPersisted(EngineCallback::CREATE_FRAMEBUFFERS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_RENDER_PASSES, this);

    m_render_list.Create(this);
    
    callbacks.TriggerPersisted(EngineCallback::CREATE_SCENES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_TEXTURES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_SHADERS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_SPATIALS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_MESHES, this);
}

void Engine::Destroy()
{
    AssertThrow(m_instance != nullptr);

    (void)m_instance->GetDevice()->Wait();

    callbacks.Trigger(EngineCallback::DESTROY_MESHES, this);
    callbacks.Trigger(EngineCallback::DESTROY_MATERIALS, this);
    callbacks.Trigger(EngineCallback::DESTROY_SKELETONS, this);
    callbacks.Trigger(EngineCallback::DESTROY_SPATIALS, this);
    callbacks.Trigger(EngineCallback::DESTROY_SHADERS, this);
    callbacks.Trigger(EngineCallback::DESTROY_TEXTURES, this);
    callbacks.Trigger(EngineCallback::DESTROY_GRAPHICS_PIPELINES, this);
    callbacks.Trigger(EngineCallback::DESTROY_COMPUTE_PIPELINES, this);
    callbacks.Trigger(EngineCallback::DESTROY_SCENES, this);

    m_deferred_renderer.Destroy(this);
    m_shadow_renderer.Destroy(this);
    m_post_processing.Destroy(this);
    
    callbacks.Trigger(EngineCallback::DESTROY_FRAMEBUFFERS, this);
    callbacks.Trigger(EngineCallback::DESTROY_RENDER_PASSES, this);

    resources.Destroy(this);

    if (shader_globals != nullptr) {
        shader_globals->scenes.Destroy(m_instance->GetDevice());
        shader_globals->objects.Destroy(m_instance->GetDevice());
        shader_globals->materials.Destroy(m_instance->GetDevice());
        shader_globals->skeletons.Destroy(m_instance->GetDevice());

        delete shader_globals;
    }

    m_instance->Destroy();
    m_instance.reset();
}

void Engine::Compile()
{
    callbacks.TriggerPersisted(EngineCallback::CREATE_SKELETONS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_MATERIALS, this);

    for (uint32_t i = 0; i < m_instance->GetFrameHandler()->NumFrames(); i++) {
        /* Finalize skeletons */
        shader_globals->skeletons.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize materials */
        shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize per-object data */
        shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize per-object data */
        shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), i);
    }

    /* Finalize descriptor pool */
    HYPERION_ASSERT_RESULT(m_instance->GetDescriptorPool().Create(m_instance->GetDevice()));

    callbacks.TriggerPersisted(EngineCallback::CREATE_GRAPHICS_PIPELINES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_COMPUTE_PIPELINES, this);
}

void Engine::UpdateDescriptorData(uint32_t frame_index)
{
    shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->skeletons.UpdateBuffer(m_instance->GetDevice(), frame_index);

    static constexpr DescriptorSet::Index bindless_descriptor_set_index[] = { DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS, DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1 };

    m_instance->GetDescriptorPool().GetDescriptorSet(bindless_descriptor_set_index[frame_index])
        ->ApplyUpdates(m_instance->GetDevice());

    shader_globals->textures.ApplyUpdates(this, frame_index);
}

void Engine::RenderShadows(CommandBuffer *primary, uint32_t frame_index)
{
    m_shadow_renderer.Render(this, primary, frame_index);
}

void Engine::RenderDeferred(CommandBuffer *primary, uint32_t frame_index)
{
    m_deferred_renderer.Render(this, primary, frame_index);
}

void Engine::RenderPostProcessing(CommandBuffer *primary, uint32_t frame_index)
{
    m_post_processing.Render(this, primary, frame_index);
}

void Engine::RenderSwapchain(CommandBuffer *command_buffer) const
{
    auto &pipeline = m_root_pipeline->Get();
    const uint32_t acquired_image_index = m_instance->GetFrameHandler()->GetAcquiredImageIndex();

    pipeline.BeginRenderPass(command_buffer, acquired_image_index);
    pipeline.Bind(command_buffer);

    m_instance->GetDescriptorPool().Bind(
        m_instance->GetDevice(),
        command_buffer,
        &pipeline,
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,
            .count = 1
        }}
    );

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    PostEffect::full_screen_quad->RenderVk(command_buffer, m_instance.get(), nullptr);

    pipeline.EndRenderPass(command_buffer, acquired_image_index);
}
} // namespace hyperion::v2