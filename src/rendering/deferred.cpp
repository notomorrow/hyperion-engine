#include "deferred.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <util/fs/fs_util.h>

namespace hyperion::v2 {

using renderer::ImageSamplerDescriptor;
using renderer::DescriptorKey;
using renderer::Rect;

DeferredPass::DeferredPass()
    : FullScreenPass(),
      m_mipmapped_result(std::make_unique<TextureImage2D>(
          Extent2D { 512, 512 },
          Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8_SRGB,
          Image::FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
          nullptr
      ))
{
}

DeferredPass::~DeferredPass() = default;

void DeferredPass::CreateShader(Engine *engine)
{
    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {
                FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_vert.spv")).Read(),
                {.name = "deferred vert"}
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/deferred_frag.spv")).Read(),
                {.name = "deferred frag"}
            }}
        }
    ));

    m_shader->Init(engine);
}

void DeferredPass::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetRenderPass().IncRef();
}

void DeferredPass::CreateDescriptors(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &framebuffer = m_framebuffers[i]->GetFramebuffer();
        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<ImageSamplerDescriptor>(DescriptorKey::DEFERRED_RESULT);

            for (auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                descriptor->SetSubDescriptor({
                    .element_index = ~0u,
                    .image_view    = attachment_ref->GetImageView(),
                    .sampler       = attachment_ref->GetSampler()
                });
            }
        }
    }
}

void DeferredPass::Create(Engine *engine)
{
    HYPERION_ASSERT_RESULT(
        m_mipmapped_result->Create(
            engine->GetDevice()
        )
    );

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].GetFramebuffers()[i].IncRef();
        
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        m_command_buffers[i] = std::move(command_buffer);
    }
    
    CreatePipeline(engine);
}

void DeferredPass::Destroy(Engine *engine)
{
    HYPERION_ASSERT_RESULT(
        m_mipmapped_result->Destroy(engine->GetDevice())
    );

    FullScreenPass::Destroy(engine); // flushes render queue
}

void DeferredPass::Render(Engine *engine, Frame *frame)
{
}

DeferredRenderer::DeferredRenderer() = default;
DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    using renderer::ImageSamplerDescriptor;

    m_post_processing.Create(engine);

    m_pass.CreateShader(engine);
    m_pass.CreateRenderPass(engine);
    m_pass.Create(engine);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto &opaque_fbo = engine->GetRenderListContainer()[Bucket::BUCKET_OPAQUE].GetFramebuffers()[i];
        
        auto *descriptor_set_pass = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);
        
        descriptor_set_pass->AddDescriptor<ImageSamplerDescriptor>(0);

        UInt attachment_index = 0;

        /* Gbuffer textures */
        for (; attachment_index < RenderListContainer::gbuffer_textures.size() - 1; attachment_index++) {
            descriptor_set_pass
                ->GetDescriptor(0)
                ->SetSubDescriptor({
                    .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetImageView(),
                    .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetSampler()
                });
        }
        
        /* Depth texture */
        descriptor_set_pass
            ->AddDescriptor<ImageSamplerDescriptor>(1)
            ->SetSubDescriptor({
                .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetImageView(),
                .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[attachment_index]->GetSampler()
            });
    }
    
    m_pass.CreateDescriptors(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void DeferredRenderer::Destroy(Engine *engine)
{
    Threads::AssertOnThread(THREAD_RENDER);

    m_post_processing.Destroy(engine);
    m_pass.Destroy(engine);  // flushes render queue
}

void DeferredRenderer::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    m_pass.Record(engine, frame_index);

    auto &render_list = engine->GetRenderListContainer();
    auto &bucket = render_list.Get(BUCKET_OPAQUE);
    
    // begin opaque objs
    bucket.GetFramebuffers()[frame_index]->BeginCapture(primary); /* TODO: frame index? */
    RenderOpaqueObjects(engine, frame);
    bucket.GetFramebuffers()[frame_index]->EndCapture(primary); /* TODO: frame index? */
    // begin opaque objs
    
    m_post_processing.RenderPre(engine, frame);

    // begin translucent objs
    m_pass.GetFramebuffer(frame_index)->BeginCapture(primary);
    /* Render deferred shading onto full screen quad */
    HYPERION_ASSERT_RESULT(m_pass.GetCommandBuffer(frame_index)->SubmitSecondary(primary));

    RenderTranslucentObjects(engine, frame);
    m_pass.GetFramebuffer(frame_index)->EndCapture(primary);
    // end translucent objs

    // mipmap chain generation
    auto *framebuffer_image = m_pass.GetFramebuffer(frame_index)->GetFramebuffer()
        .GetAttachmentRefs()[0]->GetAttachment()->GetImage();
    
    framebuffer_image->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::COPY_SRC);
    m_pass.GetMipmappedResult()->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::COPY_DST);

    // Blit into the mipmap chain img
    m_pass.GetMipmappedResult()->Blit(
        primary,
        framebuffer_image,
        Rect { 0, 0, framebuffer_image->GetExtent().width, framebuffer_image->GetExtent().height },
        Rect { 0, 0, m_pass.GetMipmappedResult()->GetExtent().width, m_pass.GetMipmappedResult()->GetExtent().height }
    );

    HYPERION_ASSERT_RESULT(m_pass.GetMipmappedResult()->GenerateMipmaps(engine->GetDevice(), primary));

    m_pass.GetMipmappedResult()->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    framebuffer_image->GetGPUImage()->InsertBarrier(primary, renderer::GPUMemory::ResourceState::SHADER_RESOURCE);

    //HYPERION_ASSERT_RESULT(m_pass.GetFramebuffer(frame_index)->GetFramebuffer()
    //    .GetAttachmentRefs()[0]->GetAttachment()->GetImage()
    //        ->GenerateMipmaps(engine->GetDevice(), frame->GetCommandBuffer()));
    
    //m_pass.GetFramebuffer(frame_index)->GetFramebuffer()
    //    .GetAttachmentRefs()[0]->GetAttachment()->GetImage()
    //        ->GetGPUImage()->InsertBarrier(frame->GetCommandBuffer(), renderer::GPUMemory::ResourceState::SHADER_RESOURCE);
    //
    //std::cout << "HasMips? " << m_pass.GetFramebuffer(frame_index)->GetFramebuffer()
    //    .GetAttachmentRefs()[0]->GetAttachment()->GetImage()->HasMipmaps() << " Num mips: " << m_pass.GetFramebuffer(frame_index)->GetFramebuffer()
    //    .GetAttachmentRefs()[0]->GetAttachment()->GetImage()->NumMipmaps() << "\n";

    m_post_processing.RenderPost(engine, frame);
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
    
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).GetGraphicsPipelines()) {
        pipeline->Render(engine, frame);
    }
}

} // namespace hyperion::v2
