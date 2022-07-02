#include "cubemap_renderer.h"

#include <util/fs/fs_util.h>

#include <engine.h>
#include <camera/perspective_camera.h>

#include <rendering/environment.h>
#include <rendering/backend/renderer_features.h>

namespace hyperion::v2 {

const std::array<std::pair<Vector3, Vector3>, 6> CubemapRenderer::cubemap_directions = {
    std::make_pair(Vector3(1, 0, 0), Vector3(0, -1, 0)),
    std::make_pair(Vector3(-1, 0, 0), Vector3(0, -1, 0)),
    std::make_pair(Vector3(0, 1, 0), Vector3(0, 0, 1)),
    std::make_pair(Vector3(0, -1, 0), Vector3(0, 0, -1)),
    std::make_pair(Vector3(0, 0, 1), Vector3(0, -1, 0)),
    std::make_pair(Vector3(0, 0, -1), Vector3(0, -1, 0))
};

CubemapRenderer::CubemapRenderer(const Extent2D &cubemap_dimensions)
    : EngineComponentBase(),
      RenderComponent(25),
      m_cubemap_dimensions(cubemap_dimensions)
{
}

CubemapRenderer::~CubemapRenderer()
{
    Teardown();
}

void CubemapRenderer::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_ANY, [this](Engine *engine) {
        for (UInt i = 0; i < 6; i++) {
            auto camera = std::make_unique<PerspectiveCamera>(
                90.0f,
                m_cubemap_dimensions.width, m_cubemap_dimensions.height,
                150.0f, 0.015f
            );

            camera->SetDirection(cubemap_directions[i].first);
            camera->SetUpVector(cubemap_directions[i].second);

            m_scenes[i] = engine->resources.scenes.Add(std::make_unique<Scene>(
                std::move(camera)
            ));
        }

        CreateImagesAndBuffers(engine);
        CreateShader(engine);
        CreateRenderPass(engine);
        CreateFramebuffers(engine);
        CreateGraphicsPipelines(engine);

        HYP_FLUSH_RENDER_QUEUE(engine);

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ANY, [this](Engine *engine) {
            m_shader       = nullptr;
            m_framebuffers = {};
            m_render_pass  = nullptr;
            m_cubemap      = nullptr;
            m_pipelines    = {};
            m_scenes       = {};

            SetReady(false);
        }), engine);
    }));
}

// called from game thread
void CubemapRenderer::InitGame(Engine *engine)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    // add all entities from environment scene
    AssertThrow(GetParent()->GetScene() != nullptr);

    for (auto &it : GetParent()->GetScene()->GetSpatials()) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        for (auto &pipeline : m_pipelines) {
            if (entity->GetRenderableAttributes().vertex_attributes &
                pipeline->GetRenderableAttributes().vertex_attributes) {

                pipeline->AddSpatial(it.second.IncRef());
            }
        }
    }
}

void CubemapRenderer::OnEntityAdded(Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    for (auto &pipeline : m_pipelines) {
        if (BucketHasGlobalIllumination(spatial->GetBucket())
            && (spatial->GetRenderableAttributes().vertex_attributes &
                pipeline->GetRenderableAttributes().vertex_attributes)) {
            pipeline->AddSpatial(spatial.IncRef());
        }
    }
}

void CubemapRenderer::OnEntityRemoved(Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    for (auto &pipeline : m_pipelines) {
        pipeline->RemoveSpatial(spatial.IncRef());
    }
}

void CubemapRenderer::OnEntityRenderableAttributesChanged(Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    const auto &renderable_attributes = spatial->GetRenderableAttributes();

    for (auto &pipeline : m_pipelines) {
        if (spatial->GetRenderableAttributes().vertex_attributes &
            pipeline->GetRenderableAttributes().vertex_attributes) {
            pipeline->AddSpatial(spatial.IncRef());
        } else {
            pipeline->RemoveSpatial(spatial.IncRef());
        }
    }
}

void CubemapRenderer::OnUpdate(Engine *engine, GameCounter::TickUnit delta)
{
    // Threads::AssertOnThread(THREAD_GAME);
    AssertReady();
}

void CubemapRenderer::OnRender(Engine *engine, Frame *frame)
{
    // Threads::AssertOnThread(THREAD_RENDER);

    auto *command_buffer = frame->GetCommandBuffer();
    const auto frame_index = frame->GetFrameIndex();

    auto result = renderer::Result::OK;


    m_framebuffers[frame_index]->BeginCapture(command_buffer);

    for (UInt i = 0; i < 6; i++) {
        engine->render_state.BindScene(m_scenes[i]);
        m_pipelines[i]->Render(engine, frame);
        engine->render_state.UnbindScene();
    }

    m_framebuffers[frame_index]->EndCapture(command_buffer);


    HYPERION_ASSERT_RESULT(result);
}

void CubemapRenderer::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    //m_shadow_pass.SetShadowMapIndex(new_index);
    AssertThrowMsg(false, "Not implemented");

    // TODO: Remove descriptor, set new descriptor
}

void CubemapRenderer::CreateImagesAndBuffers(Engine *engine)
{
    m_cubemap = engine->resources.textures.Add(std::make_unique<TextureCube>(
        m_cubemap_dimensions,
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::FilterMode::TEXTURE_FILTER_LINEAR,
        Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER,
        nullptr
    ));

    m_cubemap.Init();
}

void CubemapRenderer::CreateGraphicsPipelines(Engine *engine)
{
    for (UInt i = 0; i < 6; i++) {
        auto pipeline = std::make_unique<GraphicsPipeline>(
            m_shader.IncRef(),
            m_render_pass.IncRef(),
            RenderableAttributeSet{
                .bucket            = BUCKET_PREPASS,
                .vertex_attributes = renderer::static_mesh_vertex_attributes |
                                     renderer::skeleton_vertex_attributes
            }
        );

        pipeline->SetDepthWrite(true);
        pipeline->SetDepthTest(true);
        pipeline->SetFaceCullMode(FaceCullMode::BACK);
        pipeline->SetMultiviewIndex(i);

        for (auto &framebuffer: m_framebuffers) {
            pipeline->AddFramebuffer(framebuffer.IncRef());
        }

        m_pipelines[i] = engine->AddGraphicsPipeline(std::move(pipeline));
        m_pipelines[i].Init();
    }
}

void CubemapRenderer::CreateShader(Engine *engine)
{
    std::vector<SubShader> sub_shaders = {
        {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/vert.spv")).Read()}},
        {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/forward_frag.spv")).Read()}}
    };

    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(sub_shaders));
    m_shader->Init(engine);
}

void CubemapRenderer::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->resources.render_passes.Add(std::make_unique<RenderPass>(
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    ));

    AttachmentRef *attachment_ref;

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            m_cubemap_dimensions,
            engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_COLOR),
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        engine->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    m_render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            m_cubemap_dimensions,
            engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        engine->GetInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    m_render_pass->GetRenderPass().AddAttachmentRef(attachment_ref);

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(engine->GetInstance()->GetDevice()));
    }

    m_render_pass.Init();
}

void CubemapRenderer::CreateFramebuffers(Engine *engine)
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_framebuffers[i] = engine->resources.framebuffers.Add(std::make_unique<Framebuffer>(
            m_cubemap_dimensions,
            m_render_pass.IncRef()
        ));

        /* Add all attachments from the renderpass */
        for (auto *attachment_ref : m_render_pass->GetRenderPass().GetAttachmentRefs()) {
            m_framebuffers[i]->GetFramebuffer().AddAttachmentRef(attachment_ref);
        }

        m_framebuffers[i].Init();
    }
}

} // namespace hyperion::v2
