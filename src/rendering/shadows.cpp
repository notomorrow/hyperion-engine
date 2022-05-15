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

void ShadowEffect::SetParentScene(Scene::ID id)
{
    m_parent_scene_id = id;

    if (m_scene != nullptr) {
        m_scene->SetParentId(m_parent_scene_id);
    }
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

    pipeline->SetFaceCullMode(FaceCullMode::FRONT);
    pipeline->AddFramebuffer(m_framebuffer.IncRef());
    
    m_pipeline = engine->AddGraphicsPipeline(std::move(pipeline));
    
    for (auto &pipeline : engine->GetRenderListContainer().Get(Bucket::BUCKET_OPAQUE).graphics_pipelines) {
        m_observers.push_back(pipeline->GetSpatialNotifier().Add(Observer<Ref<Spatial>>(
            [this](Ref<Spatial> *items, size_t count) {
                for (size_t i = 0; i < count; i++) {
                    m_pipeline->AddSpatial(items[i].IncRef());
                }
            },
            [this](Ref<Spatial> *items, size_t count) {
                for (size_t i = 0; i < count; i++) {
                    m_pipeline->RemoveSpatial(items[i]->GetId());
                }
            }
        )));
    }

    m_pipeline.Init();
}

void ShadowEffect::Create(Engine *engine)
{
    m_scene = engine->resources.scenes.Add(std::make_unique<Scene>(
        std::make_unique<OrthoCamera>(
            2048, 2048,
            -1, 1,
            -1, 1,
            -1, 1
        )
    ));

    m_scene->SetParentId(m_parent_scene_id);
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

    m_observers.clear();
}

void ShadowEffect::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
}

ShadowRenderer::ShadowRenderer(Ref<Light> &&light)
    : ShadowRenderer(std::move(light), Vector3::Zero(), 25.0f)
{
}

ShadowRenderer::ShadowRenderer(Ref<Light> &&light, const Vector3 &origin, float max_distance)
    : EngineComponentBase()
{
    m_effect.SetLight(std::move(light));
    m_effect.SetOrigin(origin);
    m_effect.SetMaxDistance(max_distance);
}

ShadowRenderer::~ShadowRenderer()
{
    Teardown();
}

void ShadowRenderer::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ANY, [this](Engine *engine) {
        engine->render_scheduler.Enqueue([this, engine](...) {
            m_effect.CreateShader(engine);
            m_effect.CreateRenderPass(engine);
            m_effect.Create(engine);

            uint32_t binding_index = 12; /* TMP */
            m_effect.CreateDescriptors(engine, binding_index);

            HYPERION_RETURN_OK;
        });

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ANY, [this](Engine *engine) {
            engine->render_scheduler.Enqueue([this, engine](...) {
                m_effect.Destroy(engine);

                HYPERION_RETURN_OK;
            });

            HYP_FLUSH_RENDER_QUEUE(engine);
        }), engine);
    }));
}

void ShadowRenderer::Render(Engine *engine)
{
    UpdateSceneCamera();

    engine->render_scheduler.Enqueue([this, engine](CommandBuffer *command_buffer, uint32_t frame_index) {
        engine->render_state.BindScene(m_effect.GetScene());

        m_effect.GetFramebuffer()->BeginCapture(command_buffer);
        m_effect.GetGraphicsPipeline()->Render(engine, command_buffer, frame_index);
        m_effect.GetFramebuffer()->EndCapture(command_buffer);

        engine->render_state.UnbindScene();

        HYPERION_RETURN_OK;
    });
}

void ShadowRenderer::UpdateSceneCamera()
{
    const auto aabb = m_effect.GetAabb();
    const auto center = aabb.GetCenter();

    const auto light_direction = m_effect.GetLight() != nullptr
        ? m_effect.GetLight()->GetPosition()
        : Vector3::Zero();

    auto *camera = m_effect.GetScene()->GetCamera();

    camera->SetTranslation(center - light_direction);
    camera->SetTarget(center);
    camera->UpdateViewMatrix();

    switch (camera->GetCameraType()) {
    case CameraType::ORTHOGRAPHIC: {
        auto corners = aabb.GetCorners();

        auto maxes = MathUtil::MinSafeValue<Vector3>(),
             mins  = MathUtil::MaxSafeValue<Vector3>();

        for (auto &corner : corners) {
            corner *= camera->GetViewMatrix();

            maxes = MathUtil::Max(maxes, corner);
            mins  = MathUtil::Max(mins, corner);
        }

        static_cast<OrthoCamera *>(camera)->Set(  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            mins.x, maxes.x,
            mins.y, maxes.y,
            mins.z, maxes.z
        );

        break;
    }
    default:
        AssertThrowMsg(false, "Unhandled camera type");
    }

    camera->UpdateProjectionMatrix();
}

} // namespace hyperion::v2