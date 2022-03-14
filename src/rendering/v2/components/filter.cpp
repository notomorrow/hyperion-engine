#include "filter.h"
#include "../engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>
#include <util/mesh_factory.h>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::DescriptorSet;
using renderer::Descriptor;
using renderer::ImageSamplerDescriptor;

const MeshInputAttributeSet Filter::vertex_attributes = MeshInputAttributeSet(
    MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION
    | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
    | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
    | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
    | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
    | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT);

std::shared_ptr<Mesh> Filter::full_screen_quad = MeshFactory::CreateQuad();

Filter::Filter(Shader::ID shader_id)
    : m_pipeline_id{},
      m_framebuffer_id{},
      m_shader_id(shader_id),
      m_render_pass_id{},
      m_recorded(false)
{
}

Filter::~Filter() = default;

void Filter::CreateRenderPass(Engine *engine)
{
    /* Add the filters' renderpass */
    auto render_pass = std::make_unique<RenderPass>(RenderPass::RENDER_PASS_STAGE_SHADER, RenderPass::RENDER_PASS_SECONDARY_COMMAND_BUFFER);

    /* For our color attachment */
    render_pass->AddAttachment({
        .format = engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_COLOR)
    });

    m_render_pass_id = engine->AddRenderPass(std::move(render_pass));
}

void Filter::CreateFrameData(Engine *engine)
{
    m_frame_data = std::make_unique<PerFrameData<CommandBuffer>>(
        engine->GetInstance()->GetFrameHandler()->GetNumFrames());

    m_framebuffer_id = engine->AddFramebuffer(
        engine->GetInstance()->swapchain->extent.width,
        engine->GetInstance()->swapchain->extent.height,
        m_render_pass_id
    );

    for (size_t j = 0; j < engine->GetInstance()->GetNumImages(); j++) {
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        auto command_buffer_result = command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->command_pool
        );

        AssertThrowMsg(command_buffer_result, "%s", command_buffer_result.message);

        m_frame_data->GetFrame(j).SetCommandBuffer(std::move(command_buffer));
    }
}

void Filter::CreateDescriptors(Engine *engine, uint32_t &binding_offset)
{
    /* set descriptor */
    // TEMP: change index
    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL);

    const uint32_t num_attachments = engine->GetFramebuffer(m_framebuffer_id)->GetWrappedObject()->GetNumAttachments();

    for (uint32_t j = 0; j < num_attachments; j++) {
        descriptor_set->AddDescriptor(std::make_unique<ImageSamplerDescriptor>(
            binding_offset++,
            non_owning_ptr(engine->GetFramebuffer(m_framebuffer_id)->GetWrappedObject()->GetAttachmentImageInfos()[j].image_view.get()),
            non_owning_ptr(engine->GetFramebuffer(m_framebuffer_id)->GetWrappedObject()->GetAttachmentImageInfos()[j].sampler.get()),
            VK_SHADER_STAGE_FRAGMENT_BIT
        ));
    }
}

void Filter::CreatePipeline(Engine *engine)
{
    renderer::GraphicsPipeline::Builder builder;

    builder
        .Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) /* full screen quad is a triangle fan */
        .Shader<Shader>(m_shader_id)
        .VertexAttributes(vertex_attributes)
        .RenderPass<RenderPass>(m_render_pass_id)
        .Framebuffer<Framebuffer>(m_framebuffer_id);

    m_pipeline_id = engine->AddPipeline(std::move(builder));
}

void Filter::Destroy(Engine *engine)
{
    renderer::Result result = renderer::Result::OK;

    for (size_t i = 0; i < m_frame_data->GetNumFrames(); i++) {
        auto &frame = m_frame_data->GetFrame(i);

        HYPERION_PASS_ERRORS(
            frame.GetCommandBuffer()->Destroy(engine->GetInstance()->GetDevice(), engine->GetInstance()->command_pool),
            result
        );
    }

    m_frame_data->Reset();

    AssertThrowMsg(result, "%s", result.message);

    // engine->RemoveShader(m_shader_id);
    //engine->RemoveFramebuffer(m_framebuffer_id);
    //engine->RemovePipeline(m_pipeline_id);
    //engine->RemoveRenderPass(m_render_pass_id)
}

void Filter::Record(Engine *engine, uint32_t frame_index)
{
    using renderer::Result;

    Result result = Result::OK;

    auto *command_buffer = m_frame_data->GetFrame(frame_index).GetCommandBuffer();
    Pipeline *pipeline = engine->GetPipeline(m_pipeline_id);

    //vkDeviceWaitIdle(engine->GetInstance()->GetDevice()->GetDevice());
    //HYPERION_PASS_ERRORS(command_buffer->Reset(engine->GetInstance()->GetDevice()), result);

    HYPERION_PASS_ERRORS(
        command_buffer->Record(
            engine->GetInstance()->GetDevice(),
            pipeline->GetWrappedObject()->GetConstructionInfo().render_pass.get(),
            [this, engine, pipeline](VkCommandBuffer cmd) {
                renderer::Result result = renderer::Result::OK;

                pipeline->GetWrappedObject()->Bind(cmd);

                HYPERION_PASS_ERRORS(
                    engine->GetInstance()->GetDescriptorPool().BindDescriptorSets(cmd, pipeline->GetWrappedObject()->layout, 0, 3),
                    result
                );

                // TMP
                renderer::Frame tmp_frame;
                tmp_frame.command_buffer = cmd;

                full_screen_quad->RenderVk(&tmp_frame, engine->GetInstance(), nullptr);

                return result;
            }),
        result
    );

    AssertThrowMsg(result, "Failed to record command buffer. Message: %s", result.message);

    m_recorded = true;
}

void Filter::Render(Engine *engine, Frame *frame, uint32_t frame_index)
{
    Pipeline *pipeline = engine->GetPipeline(m_pipeline_id);

    pipeline->GetWrappedObject()->BeginRenderPass(frame->command_buffer, 0, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    
    auto *command_buffer = m_frame_data->GetFrame(frame_index).GetCommandBuffer();

    auto result = command_buffer->SubmitSecondary(frame->command_buffer);
    AssertThrowMsg(result, "%s", result.message);

    pipeline->GetWrappedObject()->EndRenderPass(frame->command_buffer, 0);
}
} // namespace hyperion