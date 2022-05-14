#include "shadows.h"
#include "../engine.h"

#include <camera/ortho_camera.h>

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

namespace hyperion::v2 {

ShadowEffect::ShadowEffect()
    : FullScreenPass()
{
}

ShadowEffect::~ShadowEffect() = default;

void ShadowEffect::CreateShader(Engine *engine)
{
    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
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

    render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }

    m_render_pass = engine->resources.render_passes.Add(std::move(render_pass));
    m_render_pass.Init();
}

void ShadowEffect::CreatePipeline(Engine *engine)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(
        std::move(m_shader),
        m_render_pass.IncRef(),
        VertexAttributeSet::static_mesh | VertexAttributeSet::skeleton,
        Bucket::BUCKET_PREPASS
    );

    pipeline->SetCullMode(CullMode::FRONT);
    pipeline->AddFramebuffer(m_framebuffer.IncRef());
    
    m_pipeline = engine->AddGraphicsPipeline(std::move(pipeline));
    
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).graphics_pipelines) {
        for (auto &spatial : pipeline->GetSpatials()) {
            if (spatial != nullptr) {
                m_pipeline->AddSpatial(spatial.IncRef());
            }
        }
    }

    m_pipeline.Init();
}

void ShadowEffect::Create(Engine *engine, std::unique_ptr<Camera> &&camera)
{
    m_scene = engine->resources.scenes.Add(std::make_unique<Scene>(std::move(camera)));
    m_scene.Init();

    auto framebuffer = std::make_unique<Framebuffer>(
        engine->GetInstance()->swapchain->extent,
        m_render_pass.IncRef()
    );

    /* Add all attachments from the renderpass */
    for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
     //   attachment_ref->SetBinding(12);
        framebuffer->GetFramebuffer().AddRenderPassAttachmentRef(attachment_ref);
    }

    m_framebuffer = engine->resources.framebuffers.Add(
        std::move(framebuffer)
    );

    m_framebuffer.Init();

    CreatePerFrameData(engine);
    CreatePipeline(engine);
}

void ShadowEffect::Destroy(Engine *engine)
{
    FullScreenPass::Destroy(engine);
}

void ShadowEffect::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
}

ShadowRenderer::ShadowRenderer(std::unique_ptr<Camera> &&camera)
    : m_camera(std::move(camera))
{
}

ShadowRenderer::~ShadowRenderer() = default;

void ShadowRenderer::Create(Engine *engine)
{
    AssertThrow(m_camera != nullptr);

    m_effect.CreateShader(engine);
    m_effect.CreateRenderPass(engine);
    m_effect.Create(engine, std::move(m_camera));

    uint32_t binding_index = 12; /* TMP */
    m_effect.CreateDescriptors(engine, binding_index);
}

void ShadowRenderer::Destroy(Engine *engine)
{
    m_effect.Destroy(engine);
}

void ShadowRenderer::Render(Engine *engine,
    CommandBuffer *primary,
    uint32_t frame_index)
{
    engine->render_bindings.BindScene(m_effect.GetScene());

    m_effect.GetFramebuffer()->BeginCapture(primary);
    m_effect.GetGraphicsPipeline()->Render(engine, primary, frame_index);
    m_effect.GetFramebuffer()->EndCapture(primary);

    engine->render_bindings.UnbindScene();
}

} // namespace hyperion::v2