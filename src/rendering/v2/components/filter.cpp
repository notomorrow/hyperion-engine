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
    auto render_pass = std::make_unique<RenderPass>(renderer::RenderPass::RENDER_PASS_STAGE_SHADER, renderer::RenderPass::RENDER_PASS_SECONDARY_COMMAND_BUFFER);

    /* For our color attachment */
    render_pass->Get().AddAttachment({
        .format = engine->GetDefaultFormat(Engine::TEXTURE_FORMAT_DEFAULT_COLOR)
    });

    m_render_pass_id = engine->AddRenderPass(std::move(render_pass));
}

void Filter::CreateFrameData(Engine *engine)
{
    const uint32_t num_frames = engine->GetInstance()->GetFrameHandler()->GetNumFrames();

    m_frame_data = std::make_unique<PerFrameData<CommandBuffer>>(num_frames);

    m_framebuffer_id = engine->AddFramebuffer(
        engine->GetInstance()->swapchain->extent.width,
        engine->GetInstance()->swapchain->extent.height,
        m_render_pass_id
    );

    for (uint32_t i = 0; i < num_frames; i++) {
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);

        auto command_buffer_result = command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        );

        AssertThrowMsg(command_buffer_result, "%s", command_buffer_result.message);

        (*m_frame_data)[i].Set<CommandBuffer>(std::move(command_buffer));
    }
}

void Filter::CreateDescriptors(Engine *engine, uint32_t &binding_offset)
{
    /* set descriptor */
    // TEMP: change index
    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL);

    const uint32_t num_attachments = engine->GetFramebuffer(m_framebuffer_id)->Get().GetNumAttachments();

    for (uint32_t i = 0; i < num_attachments; i++) {
        const auto &attachment_info = engine->GetFramebuffer(m_framebuffer_id)->Get().GetAttachmentImageInfos()[i];

        descriptor_set
            ->AddDescriptor<ImageSamplerDescriptor>(binding_offset++, VK_SHADER_STAGE_FRAGMENT_BIT)
            ->AddSubDescriptor({
                .image_view = attachment_info.image_view.get(),
                .sampler = attachment_info.sampler.get()
            });
    }
}

void Filter::CreatePipeline(Engine *engine)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(m_shader_id, m_render_pass_id);
    pipeline->AddFramebuffer(m_framebuffer_id);
    pipeline->SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);

    m_pipeline_id = engine->AddGraphicsPipeline(std::move(pipeline));
}

void Filter::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;

    AssertThrow(m_frame_data != nullptr);

    auto &frame_data = *m_frame_data;

    for (uint32_t i = 0; i < frame_data.GetNumFrames(); i++) {
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

    engine->RemoveShader(m_shader_id);
    engine->RemoveFramebuffer(m_framebuffer_id);
    engine->RemoveGraphicsPipeline(m_pipeline_id);
    engine->RemoveRenderPass(m_render_pass_id);
}

void Filter::Record(Engine *engine, uint32_t frame_index)
{
    using renderer::Result;

    auto result = Result::OK;

    auto *command_buffer = (*m_frame_data)[frame_index].Get<CommandBuffer>();
    auto *pipeline = engine->GetGraphicsPipeline(m_pipeline_id);

    HYPERION_PASS_ERRORS(
        command_buffer->Record(
            engine->GetInstance()->GetDevice(),
            pipeline->Get().GetConstructionInfo().render_pass,
            [this, engine, pipeline](CommandBuffer *cmd) {
                renderer::Result result = renderer::Result::OK;

                pipeline->Get().Bind(cmd);

                /* TODO: for testing. multiply by 0 for a red material, 1 for blue */
                uint32_t offsets[] = { 1 * sizeof(MaterialShaderData), 0 * sizeof(ObjectShaderData)};

                HYPERION_PASS_ERRORS(
                    engine->GetInstance()->GetDescriptorPool().BindDescriptorSets(cmd, &pipeline->Get(), 0, 4, std::size(offsets), offsets),
                    result
                );
                
                full_screen_quad->RenderVk(cmd, engine->GetInstance(), nullptr);

                return result;
            }),
        result
    );

    AssertThrowMsg(result, "Failed to record command buffer. Message: %s", result.message);

    m_recorded = true;
}

void Filter::Render(Engine *engine, CommandBuffer *primary_command_buffer, uint32_t frame_index)
{
    auto *pipeline = engine->GetGraphicsPipeline(m_pipeline_id);

    pipeline->Get().BeginRenderPass(primary_command_buffer, 0, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    
    auto *secondary_command_buffer = (*m_frame_data)[frame_index].Get<CommandBuffer>();

    auto result = secondary_command_buffer->SubmitSecondary(primary_command_buffer);
    AssertThrowMsg(result, "%s", result.message);

    pipeline->Get().EndRenderPass(primary_command_buffer, 0);
}
} // namespace hyperion