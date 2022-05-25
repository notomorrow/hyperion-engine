#include "deferred.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <util/fs/fs_util.h>

namespace hyperion::v2 {

using renderer::SamplerDescriptor;
using renderer::DescriptorKey;

DeferredRenderingEffect::DeferredRenderingEffect()
    : FullScreenPass()
{
    
}

DeferredRenderingEffect::~DeferredRenderingEffect() = default;

void DeferredRenderingEffect::CreateShader(Engine *engine)
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

void DeferredRenderingEffect::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].render_pass.IncRef();
}

void DeferredRenderingEffect::CreateDescriptors(Engine *engine)
{
    engine->render_scheduler.Enqueue([this, engine, &framebuffer = m_framebuffer->GetFramebuffer()](...) {
        if (!framebuffer.GetAttachmentRefs().empty()) {
            auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL);
            auto *descriptor = descriptor_set->GetOrAddDescriptor<SamplerDescriptor>(DescriptorKey::DEFERRED_RESULT);

            for (auto *attachment_ref : framebuffer.GetAttachmentRefs()) {
                descriptor->AddSubDescriptor({
                    .image_view = attachment_ref->GetImageView(),
                    .sampler    = attachment_ref->GetSampler()
                });
            }
        }

        HYPERION_RETURN_OK;
    });
}

void DeferredRenderingEffect::Create(Engine *engine)
{
    m_framebuffer = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].framebuffers[0].IncRef();

    CreatePerFrameData(engine);
    CreatePipeline(engine);
}

void DeferredRenderingEffect::Destroy(Engine *engine)
{
    FullScreenPass::Destroy(engine); // flushes render queue
}

void DeferredRenderingEffect::Render(Engine *engine, Frame *frame)
{
}

DeferredRenderer::DeferredRenderer() = default;
DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create(Engine *engine)
{
    using renderer::SamplerDescriptor;

    m_post_processing.Create(engine);

    m_effect.CreateShader(engine);
    m_effect.CreateRenderPass(engine);
    m_effect.Create(engine);

    auto &opaque_fbo = engine->GetRenderListContainer()[Bucket::BUCKET_OPAQUE].framebuffers[0];

    /* Add our gbuffer textures */
    auto *descriptor_set_pass = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL);

    /* Albedo texture */
    descriptor_set_pass
        ->AddDescriptor<SamplerDescriptor>(0)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[0]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[0]->GetSampler()
        });

    /* Normals texture*/
    descriptor_set_pass
        ->GetDescriptor(0)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[1]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[1]->GetSampler()
        });

    /* Position texture */
    descriptor_set_pass
        ->GetDescriptor(0)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[2]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[2]->GetSampler()
        });

    /* Material ID */
    descriptor_set_pass
        ->GetDescriptor(0)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[3]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[3]->GetSampler()
        });

    /* Depth texture */
    descriptor_set_pass
        ->AddDescriptor<SamplerDescriptor>(1)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[4]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetAttachmentRefs()[4]->GetSampler()
        });
    
    m_effect.CreateDescriptors(engine);

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void DeferredRenderer::Destroy(Engine *engine)
{
    m_post_processing.Destroy(engine);
    m_effect.Destroy(engine);  // flushes render queue
}

void DeferredRenderer::Render(Engine *engine, Frame *frame)
{
    auto *primary = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    m_effect.Record(engine, frame_index);

    auto &render_list = engine->GetRenderListContainer();
    auto &bucket = render_list.Get(Bucket::BUCKET_OPAQUE);

    bucket.framebuffers[0]->BeginCapture(primary); /* TODO: frame index? */
    RenderOpaqueObjects(engine, frame);
    bucket.framebuffers[0]->EndCapture(primary); /* TODO: frame index? */
    
    m_post_processing.Render(engine, primary, frame_index);
    
    m_effect.GetFramebuffer()->BeginCapture(primary);

    /* Render deferred shading onto full screen quad */
    HYPERION_ASSERT_RESULT(m_effect.GetFrameData()->At(frame_index).Get<CommandBuffer>()->SubmitSecondary(primary));

    RenderTranslucentObjects(engine, frame);
    
    m_effect.GetFramebuffer()->EndCapture(primary);
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).graphics_pipelines) {
        pipeline->Render(engine, frame);
    }
    
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).graphics_pipelines) {
        pipeline->Render(engine, frame);
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, Frame *frame)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).graphics_pipelines) {
        pipeline->Render(engine, frame);
    }
}

} // namespace hyperion::v2