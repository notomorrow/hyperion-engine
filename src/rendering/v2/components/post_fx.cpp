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

PostEffect::PostEffect(Shader::ID shader_id)
    : m_pipeline_id{},
      m_framebuffer_id{},
      m_shader_id(shader_id),
      m_render_pass_id{}
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

    m_render_pass_id = engine->resources.render_passes.Add(engine, std::move(render_pass));
}

void PostEffect::Create(Engine *engine)
{
    auto *render_pass = engine->resources.render_passes[m_render_pass_id];

    AssertThrow(render_pass != nullptr);

    auto framebuffer = std::make_unique<Framebuffer>(engine->GetInstance()->swapchain->extent);

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
    
    engine->GetCallbacks(Engine::CALLBACK_GRAPHICS_PIPELINES).on_init += [this](Engine *engine) {
        CreatePipeline(engine);
    };

    engine->GetCallbacks(Engine::CALLBACK_GRAPHICS_PIPELINES).on_deinit += [this](Engine *engine) {
        DestroyPipeline(engine);
    };
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
    auto &framebuffer = engine->resources.framebuffers[m_framebuffer_id]->Get();
    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_PASS);
    
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
    auto pipeline = std::make_unique<GraphicsPipeline>(m_shader_id, m_render_pass_id, GraphicsPipeline::Bucket::BUCKET_PREPASS);
    pipeline->AddFramebuffer(m_framebuffer_id);
    pipeline->SetDepthWrite(false);
    pipeline->SetDepthTest(false);
    pipeline->SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);

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

    AssertThrowMsg(result, "%s", result.message);

    engine->resources.shaders.Remove(engine, m_shader_id);
    engine->resources.framebuffers.Remove(engine, m_framebuffer_id);
    engine->resources.render_passes.Remove(engine, m_render_pass_id);
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
            [this, engine, pipeline](CommandBuffer *cmd) {
                pipeline->Get().Bind(cmd);
                
                HYPERION_BUBBLE_ERRORS(engine->GetInstance()->GetDescriptorPool().Bind(
                    engine->GetInstance()->GetDevice(),
                    cmd,
                    &pipeline->Get(),
                    {
                        {.count = 3},
                        {},
                        {.offsets = {uint32_t(0)}}
                    }
                ));

                full_screen_quad->RenderVk(cmd, engine->GetInstance(), nullptr);

                HYPERION_RETURN_OK;
            }),
        result
    );

    AssertThrowMsg(result, "Failed to record command buffer. Message: %s", result.message);
}

void PostEffect::Render(Engine * engine, CommandBuffer *primary_command_buffer, uint32_t frame_index)
{
    auto *pipeline = engine->GetGraphicsPipeline(m_pipeline_id);

    pipeline->Get().BeginRenderPass(primary_command_buffer, 0);

    auto *secondary_command_buffer = m_frame_data->At(frame_index).Get<CommandBuffer>();

    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(primary_command_buffer));

    pipeline->Get().EndRenderPass(primary_command_buffer, 0);
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
        Shader::ID shader_id = engine->resources.shaders.Add(engine, std::make_unique<Shader>(std::vector<SubShader>{
            SubShader{ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_vert.spv").Read()}},
            SubShader{ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/" + filter_shader_names[i] + "_frag.spv").Read()}}
        }));

        m_filters[i] = std::make_unique<PostEffect>(shader_id);
        m_filters[i]->CreateRenderPass(engine);
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