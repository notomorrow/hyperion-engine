#include "post_fx.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>
#include <util/mesh_factory.h>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;
using renderer::Descriptor;
using renderer::DescriptorSet;
using renderer::ImageSamplerDescriptor;

std::shared_ptr<hyperion::Mesh> PostEffect::full_screen_quad = MeshFactory::CreateQuad();

PostEffect::PostEffect()
    : PostEffect(nullptr)
{
}

PostEffect::PostEffect(Ref<Shader> &&shader)
    : m_pipeline_id{},
      m_shader(std::move(shader)),
      m_render_pass(nullptr)
{
}

PostEffect::~PostEffect() = default;

void PostEffect::CreateRenderPass(Engine *engine)
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
            engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_COLOR),
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

    m_render_pass = engine->resources.render_passes.Add(std::move(render_pass));
    m_render_pass.Init();
}

void PostEffect::Create(Engine *engine)
{
    CreateRenderPass(engine);

    m_framebuffer = engine->resources.framebuffers.Add(std::make_unique<Framebuffer>(
        engine->GetInstance()->swapchain->extent,
        m_render_pass.Acquire()
    ));

    /* Add all attachments from the renderpass */
    for (auto *attachment_ref : m_render_pass->Get().GetRenderPassAttachmentRefs()) {
        m_framebuffer->Get().AddRenderPassAttachmentRef(attachment_ref);
    }
    
    m_framebuffer.Init();

    CreatePerFrameData(engine);

    engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this, engine](...) {
        CreatePipeline(engine);
    });

    engine->callbacks.Once(EngineCallback::DESTROY_GRAPHICS_PIPELINES, [this, engine](...) {
        DestroyPipeline(engine);
    });
}

void PostEffect::CreatePerFrameData(Engine *engine)
{
    const uint32_t num_frames = engine->GetInstance()->GetFrameHandler()->NumFrames();

    m_frame_data = std::make_unique<PerFrameData<CommandBuffer>>(num_frames);

    for (uint32_t i = 0; i < num_frames; i++) {
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        m_frame_data->At(i).Set<CommandBuffer>(std::move(command_buffer));
    }
}

void PostEffect::CreateDescriptors(Engine *engine, uint32_t &binding_offset)
{
    /* set descriptor */
    auto &framebuffer = m_framebuffer->Get();
    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL);
    
    for (auto *attachment_ref : framebuffer.GetRenderPassAttachmentRefs()) {
        descriptor_set
            ->AddDescriptor<ImageSamplerDescriptor>(binding_offset++)
            ->AddSubDescriptor({
                .image_view = attachment_ref->GetImageView(),
                .sampler    = attachment_ref->GetSampler()
            });
   }
}

void PostEffect::CreatePipeline(Engine *engine)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(
        std::move(m_shader),
        nullptr,
        m_render_pass.Acquire(),
        GraphicsPipeline::Bucket::BUCKET_PREPASS
    );

    pipeline->AddFramebuffer(m_framebuffer.Acquire());
    pipeline->SetDepthWrite(false);
    pipeline->SetDepthTest(false);
    pipeline->SetTopology(Topology::TRIANGLE_FAN);

    m_pipeline_id = engine->AddGraphicsPipeline(std::move(pipeline));
}

void PostEffect::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;

    AssertThrow(m_frame_data != nullptr);

    auto &frame_data = *m_frame_data;

    for (uint32_t i = 0; i < frame_data.NumFrames(); i++) {
        HYPERION_PASS_ERRORS(
            frame_data[i].Get<CommandBuffer>()->Destroy(
                engine->GetInstance()->GetDevice(),
                engine->GetInstance()->GetGraphicsCommandPool()
            ),
            result
        );
    }

    m_frame_data->Reset();

    m_framebuffer = nullptr;

    for (auto &attachment : m_attachments) {
        HYPERION_PASS_ERRORS(attachment->Destroy(engine->GetInstance()->GetDevice()), result);
    }

    HYPERION_ASSERT_RESULT(result);
}

void PostEffect::DestroyPipeline(Engine *engine)
{
    engine->RemoveGraphicsPipeline(m_pipeline_id);
}

void PostEffect::Record(Engine *engine, uint32_t frame_index)
{
    using renderer::Result;

    auto result = Result::OK;

    auto *command_buffer = m_frame_data->At(frame_index).Get<CommandBuffer>();
    auto *pipeline = engine->GetGraphicsPipeline(m_pipeline_id);

    HYPERION_PASS_ERRORS(
        command_buffer->Record(
            engine->GetInstance()->GetDevice(),
            pipeline->Get().GetConstructionInfo().render_pass,
            [this, engine, pipeline, frame_index](CommandBuffer *cmd) {
                pipeline->Get().Bind(cmd);
                
                HYPERION_BUBBLE_ERRORS(engine->GetInstance()->GetDescriptorPool().Bind(
                    engine->GetInstance()->GetDevice(),
                    cmd,
                    &pipeline->Get(),
                    {
                        {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL, .count = 1},
                        {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL}
                    }
                ));
                
                HYPERION_BUBBLE_ERRORS(engine->GetInstance()->GetDescriptorPool().Bind(
                    engine->GetInstance()->GetDevice(),
                    cmd,
                    &pipeline->Get(),
                    {
                        {.set = DescriptorSet::scene_buffer_mapping[frame_index], .count = 1},
                        {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
                        {.offsets = {uint32_t(0 * sizeof(SceneShaderData))}} /* TODO: scene index */
                    }
                ));
                
                HYPERION_BUBBLE_ERRORS(engine->GetInstance()->GetDescriptorPool().Bind(
                    engine->GetInstance()->GetDevice(),
                    cmd,
                    &pipeline->Get(),
                    {
                        {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
                        {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
                    }
                ));

                full_screen_quad->RenderVk(cmd, engine->GetInstance(), nullptr);

                HYPERION_RETURN_OK;
            }),
        result
    );

    AssertThrowMsg(result, "Failed to record command buffer. Message: %s", result.message);
}

void PostEffect::Render(Engine * engine, CommandBuffer *primary, uint32_t frame_index)
{
    m_framebuffer->BeginCapture(primary);

    auto *secondary_command_buffer = m_frame_data->At(frame_index).Get<CommandBuffer>();

    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(primary));

    m_framebuffer->EndCapture(primary);
}


PostProcessing::PostProcessing()
    : m_filters{}
{
}

PostProcessing::~PostProcessing() = default;

void PostProcessing::Create(Engine *engine)
{
    /* Add a renderpass per each filter */
    static const std::array<std::string, 1> filter_shader_names = {  "filter_pass" };
    m_filters.resize(filter_shader_names.size());

    uint32_t binding_index = 8; /* hardcoded for now - start filters at this binding */

    /* TODO: use subpasses for gbuffer so we only have num_filters * num_frames descriptors */

    for (int i = 0; i < filter_shader_names.size(); i++) {
        m_filters[i] = std::make_unique<PostEffect>(engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                SubShader{ShaderModule::Type::VERTEX, {
                    FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_vert.spv").Read(),
                    {.name = "filter vert"}
                }},
                SubShader{ShaderModule::Type::FRAGMENT, {
                    FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_frag.spv").Read(),
                    {.name = "filter frag"}
                }}
            }
        )));
        
        m_filters[i]->Create(engine);
        m_filters[i]->CreateDescriptors(engine, binding_index);
    }
}

void PostProcessing::Destroy(Engine *engine)
{
    for (auto &filter : m_filters) {
        filter->Destroy(engine);
    }
}

void PostProcessing::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index) const
{
    for (auto &filter : m_filters) {
        filter->Record(engine, frame_index);
        filter->Render(engine, primary, frame_index);
    }
}
} // namespace hyperion