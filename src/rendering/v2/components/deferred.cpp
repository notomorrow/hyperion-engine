#include "deferred.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

namespace hyperion::v2 {

DeferredRenderingEffect::DeferredRenderingEffect()
    : PostEffect()
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
    m_render_pass = engine->GetRenderList()[GraphicsPipeline::BUCKET_TRANSLUCENT].render_pass.Acquire();
}

void DeferredRenderingEffect::Create(Engine *engine)
{
    m_framebuffer = engine->GetRenderList()[GraphicsPipeline::BUCKET_TRANSLUCENT].framebuffers[0].Acquire();

    CreatePerFrameData(engine);

    engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this, engine](...) {
        CreatePipeline(engine);
    });

    engine->callbacks.Once(EngineCallback::DESTROY_GRAPHICS_PIPELINES, [this, engine](...) {
        DestroyPipeline(engine);
    });
}

void DeferredRenderingEffect::Destroy(Engine *engine)
{
    PostEffect::Destroy(engine);
}

void DeferredRenderingEffect::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
}

DeferredRenderer::DeferredRenderer() = default;
DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Create(Engine *engine)
{
    using renderer::ImageSamplerDescriptor;

    m_effect.CreateShader(engine);
    m_effect.CreateRenderPass(engine);
    m_effect.Create(engine);

    auto &opaque_fbo = engine->GetRenderList()[GraphicsPipeline::BUCKET_OPAQUE].framebuffers[0];

    /* Add our gbuffer textures */
    auto *descriptor_set_pass = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL);

    /* Albedo texture */
    descriptor_set_pass
        ->AddDescriptor<ImageSamplerDescriptor>(0)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->Get().GetRenderPassAttachmentRefs()[0]->GetImageView(),
            .sampler    = opaque_fbo->Get().GetRenderPassAttachmentRefs()[0]->GetSampler()
        });

    /* Normals texture*/
    descriptor_set_pass
        ->AddDescriptor<ImageSamplerDescriptor>(1)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->Get().GetRenderPassAttachmentRefs()[1]->GetImageView(),
            .sampler    = opaque_fbo->Get().GetRenderPassAttachmentRefs()[1]->GetSampler()
        });

    /* Position texture */
    descriptor_set_pass
        ->AddDescriptor<ImageSamplerDescriptor>(2)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->Get().GetRenderPassAttachmentRefs()[2]->GetImageView(),
            .sampler    = opaque_fbo->Get().GetRenderPassAttachmentRefs()[2]->GetSampler()
        });

    /* Depth texture */
    descriptor_set_pass
        ->AddDescriptor<ImageSamplerDescriptor>(3)
        ->AddSubDescriptor({
            .image_view = opaque_fbo->Get().GetRenderPassAttachmentRefs()[3]->GetImageView(),
            .sampler    = opaque_fbo->Get().GetRenderPassAttachmentRefs()[3]->GetSampler()
        });

    uint32_t binding_index = 4; /* TMP */
    m_effect.CreateDescriptors(engine, binding_index);
}

void DeferredRenderer::Destroy(Engine *engine)
{
    m_effect.Destroy(engine);
}

void DeferredRenderer::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    m_effect.Record(engine, frame_index);

    auto &render_list = engine->GetRenderList();
    render_list.Get(GraphicsPipeline::Bucket::BUCKET_OPAQUE).BeginRenderPass(engine, primary, 0);
    RenderOpaqueObjects(engine, primary, frame_index);
    render_list.Get(GraphicsPipeline::Bucket::BUCKET_OPAQUE).EndRenderPass(engine, primary);

    auto *pipeline = engine->GetGraphicsPipeline(m_effect.GetGraphicsPipelineId());
    pipeline->Get().BeginRenderPass(primary, 0);

    auto *secondary_command_buffer = m_effect.GetFrameData()->At(frame_index).Get<CommandBuffer>();
    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(primary));

    RenderTranslucentObjects(engine, primary, frame_index);

    pipeline->Get().EndRenderPass(primary, 0);
}

void DeferredRenderer::RenderOpaqueObjects(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    auto &render_list = engine->GetRenderList();

    for (const auto &pipeline : render_list.Get(GraphicsPipeline::Bucket::BUCKET_SKYBOX).pipelines.objects) {
        pipeline->Render(engine, primary, frame_index);
    }

    for (const auto &pipeline : render_list.Get(GraphicsPipeline::Bucket::BUCKET_OPAQUE).pipelines.objects) {
        pipeline->Render(engine, primary, frame_index);
    }
}

void DeferredRenderer::RenderTranslucentObjects(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    const auto &render_list = engine->GetRenderList();

    for (const auto &pipeline : render_list[GraphicsPipeline::Bucket::BUCKET_TRANSLUCENT].pipelines.objects) {
        pipeline->Render(engine, primary, frame_index);
    }
}

} // namespace hyperion::v2