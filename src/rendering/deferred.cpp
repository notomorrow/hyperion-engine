#include "deferred.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

namespace hyperion::v2 {

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
                FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/deferred_vert.spv").Read(),
                {.name = "deferred vert"}
            }},
            SubShader{ShaderModule::Type::FRAGMENT, {
                FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/deferred_frag.spv").Read(),
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

void DeferredRenderingEffect::Create(Engine *engine)
{
    m_framebuffer = engine->GetRenderListContainer()[Bucket::BUCKET_TRANSLUCENT].framebuffers[0].IncRef();

    CreatePerFrameData(engine);
    CreatePipeline(engine);
}

void DeferredRenderingEffect::Destroy(Engine *engine)
{
    FullScreenPass::Destroy(engine);
}

void DeferredRenderingEffect::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
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
            .image_view = opaque_fbo->GetFramebuffer().GetRenderPassAttachmentRefs()[0]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetRenderPassAttachmentRefs()[0]->GetSampler()
        });

    /* Normals texture*/
    descriptor_set_pass
        ->GetDescriptor(0)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetRenderPassAttachmentRefs()[1]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetRenderPassAttachmentRefs()[1]->GetSampler()
        });

    /* Position texture */
    descriptor_set_pass
        ->GetDescriptor(0)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetRenderPassAttachmentRefs()[2]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetRenderPassAttachmentRefs()[2]->GetSampler()
        });

    /* Material ID */
    descriptor_set_pass
        ->GetDescriptor(0)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetRenderPassAttachmentRefs()[3]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetRenderPassAttachmentRefs()[3]->GetSampler()
        });

    /* Depth texture */
    descriptor_set_pass
        ->AddDescriptor<SamplerDescriptor>(1)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->GetFramebuffer().GetRenderPassAttachmentRefs()[4]->GetImageView(),
            .sampler    = opaque_fbo->GetFramebuffer().GetRenderPassAttachmentRefs()[4]->GetSampler()
        });

    uint32_t binding_index = 4; /* TMP */
    m_effect.CreateDescriptors(engine, binding_index);
}

void DeferredRenderer::Destroy(Engine *engine)
{
    m_post_processing.Destroy(engine);
    m_effect.Destroy(engine);
}

void DeferredRenderer::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    m_effect.Record(engine, frame_index);

    auto &render_list = engine->GetRenderListContainer();
    auto &bucket = render_list.Get(Bucket::BUCKET_OPAQUE);

    bucket.framebuffers[0]->BeginCapture(primary); /* TODO: frame index? */
    RenderOpaqueObjects(engine, primary, frame_index);
    bucket.framebuffers[0]->EndCapture(primary); /* TODO: frame index? */
    
    m_post_processing.Render(engine, primary, frame_index);
    
    m_effect.GetFramebuffer()->BeginCapture(primary);

    /* Render deferred shading onto full screen quad */
    HYPERION_ASSERT_RESULT(m_effect.GetFrameData()->At(frame_index).Get<CommandBuffer>()->SubmitSecondary(primary));

    RenderTranslucentObjects(engine, primary, frame_index);
    
    m_effect.GetFramebuffer()->EndCapture(primary);
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_SKYBOX).graphics_pipelines) {
        pipeline->Render(engine, primary, frame_index);
    }
    
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).graphics_pipelines) {
        pipeline->Render(engine, primary, frame_index);
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_TRANSLUCENT).graphics_pipelines) {
        pipeline->Render(engine, primary, frame_index);
    }
}

} // namespace hyperion::v2