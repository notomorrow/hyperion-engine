#include "deferred.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

namespace hyperion::v2 {

DeferredRenderingEffect::DeferredRenderingEffect()
    : PostEffect(Shader::ID{})
{
    
}

DeferredRenderingEffect::~DeferredRenderingEffect() = default;

void DeferredRenderingEffect::CreateShader(Engine *engine)
{
    m_shader_id = engine->resources.shaders.Add(engine, std::make_unique<Shader>(std::vector<SubShader>{
        SubShader{ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/deferred_vert.spv").Read()}},
        SubShader{ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/deferred_frag.spv").Read()}}
    }));
}

void DeferredRenderingEffect::CreateRenderPass(Engine *engine)
{
    m_render_pass_id = engine->GetRenderList()[GraphicsPipeline::BUCKET_TRANSLUCENT].render_pass_id;
}

void DeferredRenderingEffect::Create(Engine *engine)
{
    m_framebuffer_id = engine->GetRenderList()[GraphicsPipeline::BUCKET_TRANSLUCENT].framebuffer_ids[0];

    CreatePerFrameData(engine);

    engine->GetCallbacks(Engine::CALLBACK_GRAPHICS_PIPELINES).on_init += [this](Engine *engine) {
        CreatePipeline(engine);
    };

    engine->GetCallbacks(Engine::CALLBACK_GRAPHICS_PIPELINES).on_deinit += [this](Engine *engine) {
        DestroyPipeline(engine);
    };
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
    m_effect.CreateShader(engine);
    m_effect.CreateRenderPass(engine);
    m_effect.Create(engine);

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

    RenderTransparentObjects(engine, primary, frame_index);

    pipeline->Get().EndRenderPass(primary, 0);
}

} // namespace hyperion::v2