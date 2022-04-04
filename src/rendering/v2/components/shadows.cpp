#include "shadows.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

namespace hyperion::v2 {

ShadowEffect::ShadowEffect()
    : PostEffect(Shader::ID{})
{

}

ShadowEffect::~ShadowEffect() = default;

void ShadowEffect::CreateShader(Engine *engine)
{
    m_shader_id = engine->AddShader(std::make_unique<Shader>(std::vector<SubShader>{
        SubShader{ ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/vert.spv").Read()} },
        SubShader{ ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/shadow_frag.spv").Read()} }
    }));
}

void ShadowEffect::CreateRenderPass(Engine *engine)
{
    /* Add the filters' renderpass */
    auto render_pass = std::make_unique<RenderPass>(renderer::RenderPass::Stage::RENDER_PASS_STAGE_SHADER, renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER);
    
    /* For our shadow map depth attachment */
    render_pass->Get().AddAttachment({
        .format = engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_DEPTH)
    });

    m_render_pass_id = engine->AddRenderPass(std::move(render_pass));
}

void ShadowEffect::CreatePipeline(Engine *engine)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(m_shader_id, m_render_pass_id, GraphicsPipeline::Bucket::BUCKET_PREPASS);
    pipeline->AddFramebuffer(m_framebuffer_id);
    pipeline->SetSceneIndex(1);

    m_pipeline_id = engine->AddGraphicsPipeline(std::move(pipeline));
}

void ShadowEffect::Create(Engine *engine)
{
    m_framebuffer_id = engine->AddFramebuffer(
        engine->GetInstance()->swapchain->extent.width,
        engine->GetInstance()->swapchain->extent.height,
        m_render_pass_id
    );

    CreatePerFrameData(engine);
    
    engine->GetCallbacks(Engine::CALLBACK_GRAPHICS_PIPELINES).on_init += [this](Engine *engine) {
        CreatePipeline(engine);
    };

    engine->GetCallbacks(Engine::CALLBACK_GRAPHICS_PIPELINES).on_deinit += [this](Engine *engine) {
        DestroyPipeline(engine);
    };
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
    engine->GetCallbacks(Engine::CALLBACK_GRAPHICS_PIPELINES).on_init += [this](Engine *engine) {
        auto *pipeline = engine->GetGraphicsPipeline(m_effect.GetGraphicsPipelineId());

        for (auto &opaque_pipeline : engine->GetRenderList()[GraphicsPipeline::Bucket::BUCKET_OPAQUE].pipelines.objects) {
            for (Spatial *spatial : opaque_pipeline->GetSpatials()) {
                if (spatial != nullptr) {
                    pipeline->AddSpatial(engine, spatial);
                }
            }
        }
    };
}

void ShadowRenderer::Destroy(Engine *engine)
{
    m_effect.Destroy(engine);
}

void ShadowRenderer::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    using renderer::Result;
    
    auto *pipeline = engine->GetGraphicsPipeline(m_effect.GetGraphicsPipelineId());

    pipeline->Get().BeginRenderPass(primary, 0);

    pipeline->Render(engine, primary, frame_index);

    pipeline->Get().EndRenderPass(primary, 0);
}

} // namespace hyperion::v2