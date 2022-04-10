#include "shadows.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

namespace hyperion::v2 {

ShadowEffect::ShadowEffect()
    : PostEffect({})
{
}

ShadowEffect::~ShadowEffect() = default;

void ShadowEffect::CreateShader(Engine *engine)
{
    m_shader = engine->resources.shaders.Create(std::make_unique<Shader>(
        std::vector<SubShader>{
            SubShader{ ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/vert.spv").Read()} },
            SubShader{ ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/shadow_frag.spv").Read()} }
        }
    ));

    m_shader->Init(engine);
}

void ShadowEffect::CreateRenderPass(Engine *engine)
{
    /* Add the filters' renderpass */
    auto render_pass = std::make_unique<RenderPass>(
        renderer::RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    renderer::AttachmentRef *attachment_ref;

    m_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            engine->GetInstance()->swapchain->extent,
            engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        renderer::RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        engine->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    render_pass->Get().AddRenderPassAttachmentRef(attachment_ref);

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }

    m_render_pass_id = engine->resources.render_passes.Add(engine, std::move(render_pass));
}

void ShadowEffect::CreatePipeline(Engine *engine)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(
        std::move(m_shader),
        m_render_pass_id,
        GraphicsPipeline::Bucket::BUCKET_PREPASS
    );
    pipeline->SetCullMode(CullMode::FRONT);
    pipeline->AddFramebuffer(m_framebuffer_id);
    pipeline->SetSceneIndex(1);

    m_pipeline_id = engine->AddGraphicsPipeline(std::move(pipeline));
}

void ShadowEffect::Create(Engine *engine)
{
    auto *render_pass = engine->resources.render_passes[m_render_pass_id];

    AssertThrow(render_pass != nullptr);

    auto framebuffer = std::make_unique<Framebuffer>(
        engine->GetInstance()->swapchain->extent
    );

    /* Add all attachments from the renderpass */
    for (auto *attachment_ref : render_pass->Get().GetRenderPassAttachmentRefs()) {
        framebuffer->Get().AddRenderPassAttachmentRef(attachment_ref);
    }

    m_framebuffer_id = engine->resources.framebuffers.Add(
        engine,
        std::move(framebuffer),
        &render_pass->Get()
    );

    CreatePerFrameData(engine);

    engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this, engine](...) {
        CreatePipeline(engine);
    });

    engine->callbacks.Once(EngineCallback::DESTROY_GRAPHICS_PIPELINES, [this, engine](...) {
        DestroyPipeline(engine);
    });
}

void ShadowEffect::Destroy(Engine *engine)
{
    PostEffect::Destroy(engine);
}

void ShadowEffect::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
}

ShadowRenderer::ShadowRenderer() = default;
ShadowRenderer::~ShadowRenderer() = default;

void ShadowRenderer::Create(Engine *engine)
{
    m_effect.CreateShader(engine);
    m_effect.CreateRenderPass(engine);
    m_effect.Create(engine);

    uint32_t binding_index = 9; /* TMP */
    m_effect.CreateDescriptors(engine, binding_index);

    /* TMP */
    engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this, engine](...) {
        auto *pipeline = engine->GetGraphicsPipeline(m_effect.GetGraphicsPipelineId());

        for (auto &opaque_pipeline : engine->GetRenderList()[GraphicsPipeline::Bucket::BUCKET_OPAQUE].pipelines.objects) {
            for (auto &spatial : opaque_pipeline->GetSpatials()) {
                if (spatial != nullptr) {
                    pipeline->AddSpatial(spatial.Acquire());
                }
            }
        }
    });
}

void ShadowRenderer::Destroy(Engine *engine)
{
    m_effect.Destroy(engine);
}

void ShadowRenderer::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    using renderer::Result;
    
    auto *pipeline = engine->GetGraphicsPipeline(m_effect.GetGraphicsPipelineId());

    AssertThrow(pipeline != nullptr);

    pipeline->Get().BeginRenderPass(primary, 0);
    pipeline->Render(engine, primary, frame_index);
    pipeline->Get().EndRenderPass(primary, 0);
}

} // namespace hyperion::v2