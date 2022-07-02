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

CubemapRenderer::CubemapRenderer(
    const Extent2D &cubemap_dimensions,
    const Vector3 &origin
) : EngineComponentBase(),
    RenderComponent(25),
    m_cubemap_dimensions(cubemap_dimensions),
    m_origin(origin)
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
        //for (UInt i = 0; i < 6; i++) {
            auto camera = std::make_unique<PerspectiveCamera>(
                90.0f,
                m_cubemap_dimensions.width, m_cubemap_dimensions.height,
                150.0f, 0.015f
            );

            //camera->SetDirection(cubemap_directions[i].first);
            //camera->SetUpVector(cubemap_directions[i].second);

            m_scene = engine->resources.scenes.Add(std::make_unique<Scene>(
                std::move(camera)
            ));
        //}

        CreateShader(engine);
        CreateRenderPass(engine);
        CreateFramebuffers(engine);
        CreateImagesAndBuffers(engine);
        CreateGraphicsPipelines(engine);

        HYP_FLUSH_RENDER_QUEUE(engine);

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_ANY, [this](Engine *engine) {
            // destroy descriptors

            for (UInt i = 0; i < max_frames_in_flight; i++) {
                if (m_framebuffers[i] != nullptr) {
                    for (auto &attachment : m_attachments) {
                        m_framebuffers[i]->RemoveAttachmentRef(attachment.get());
                    }

                    if (m_pipeline != nullptr) {
                        m_pipeline->RemoveFramebuffer(m_framebuffers[i]->GetId());
                    }
                }
            }

            if (m_render_pass != nullptr) {
                for (auto &attachment : m_attachments) {
                    m_render_pass->GetRenderPass().RemoveAttachmentRef(attachment.get());
                }
            }

            engine->render_scheduler.Enqueue([this, engine](...) {
                auto result = renderer::Result::OK;

                for (UInt i = 0; i < max_frames_in_flight; i++) {
                    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
                        .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);

                    descriptor_set->RemoveDescriptor(DescriptorKey::CUBEMAP_UNIFORMS);
                }
                for (auto &attachment : m_attachments) {
                    HYPERION_PASS_ERRORS(attachment->Destroy(engine->GetInstance()->GetDevice()), result);
                }

                m_attachments.clear();

                return m_uniform_buffer.Destroy(engine->GetDevice());
            });


            m_shader       = nullptr;
            m_framebuffers = {};
            m_render_pass  = nullptr;
            m_cubemap      = nullptr;
            m_pipeline     = nullptr;
            m_scene        = nullptr;

            HYP_FLUSH_RENDER_QUEUE(engine);

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

       // for (auto &pipeline : m_pipelines) {
            if (entity->GetRenderableAttributes().vertex_attributes &
                m_pipeline->GetRenderableAttributes().vertex_attributes) {

                m_pipeline->AddSpatial(it.second.IncRef());
            }
       // }
    }
}

void CubemapRenderer::OnEntityAdded(Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    //for (auto &pipeline : m_pipelines) {
        if (spatial->GetRenderableAttributes().vertex_attributes &
            m_pipeline->GetRenderableAttributes().vertex_attributes) {
            m_pipeline->AddSpatial(spatial.IncRef());
        }
    //}
}

void CubemapRenderer::OnEntityRemoved(Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    //for (auto &pipeline : m_pipelines) {
        m_pipeline->RemoveSpatial(spatial.IncRef());
    //}
}

void CubemapRenderer::OnEntityRenderableAttributesChanged(Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    //for (auto &pipeline : m_pipelines) {
        if (spatial->GetRenderableAttributes().vertex_attributes &
            m_pipeline->GetRenderableAttributes().vertex_attributes) {
            m_pipeline->AddSpatial(spatial.IncRef());
        } else {
            m_pipeline->RemoveSpatial(spatial.IncRef());
        }
    //}
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

    m_pipeline->GetPipeline()->push_constants = {
        .render_component_data = {
            .index = GetComponentIndex()
        }
    };

    engine->render_state.BindScene(m_scene);
    m_pipeline->Render(engine, frame);
    engine->render_state.UnbindScene();

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
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8,
        Image::FilterMode::TEXTURE_FILTER_LINEAR,
        Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER,
        nullptr
    ));

    m_cubemap.Init();

    CubemapUniforms cubemap_uniforms{};

    for (UInt i = 0; i < 6; i++) {
        cubemap_uniforms.projection_matrices[i] = Matrix4::Perspective(
            90.0f,
            m_cubemap_dimensions.width, m_cubemap_dimensions.height,
            0.015f, 150.0f
        );

        cubemap_uniforms.view_matrices[i] = Matrix4::LookAt(
            m_origin,
            cubemap_directions[i].first,
            cubemap_directions[i].second
        );
    }

    m_uniform_buffer.Create(engine->GetDevice(), sizeof(CubemapUniforms));
    m_uniform_buffer.Copy(engine->GetDevice(), sizeof(CubemapUniforms), &cubemap_uniforms);

    // create descriptors
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
            .GetDescriptorSet(DescriptorSet::global_buffer_mapping[i]);

        descriptor_set
            ->GetOrAddDescriptor<renderer::UniformBufferDescriptor>(DescriptorKey::CUBEMAP_UNIFORMS)
            ->SetSubDescriptor({
                .element_index = GetComponentIndex(),
                .buffer        = &m_uniform_buffer
            });

        descriptor_set
            ->GetOrAddDescriptor<renderer::ImageDescriptor>(DescriptorKey::CUBEMAP_TEST)
            ->SetSubDescriptor({
                .element_index = GetComponentIndex(),
                .image_view    = GetCubemapImageView(i)
            });
    }

    DebugLog(
        LogType::Debug,
        "Added cubemap uniform buffers\n"
    );
}

void CubemapRenderer::CreateGraphicsPipelines(Engine *engine)
{
    //for (UInt i = 0; i < 6; i++) {
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
        pipeline->SetMultiviewIndex(0);

        for (auto &framebuffer: m_framebuffers) {
            pipeline->AddFramebuffer(framebuffer.IncRef());
        }

        m_pipeline = engine->AddGraphicsPipeline(std::move(pipeline));
        DebugLog(LogType::Debug, "Create cubemap pipeline\n");
        m_pipeline.Init();
   // }
}

void CubemapRenderer::CreateShader(Engine *engine)
{
    std::vector<SubShader> sub_shaders = {
        {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/cubemap_renderer.vert.spv")).Read()}},
        {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "/vkshaders/cubemap_renderer.frag.spv")).Read()}}
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
