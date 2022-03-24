#include "graphics.h"
#include "../engine.h"

#include <rendering/backend/renderer_descriptor.h>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;

GraphicsPipeline::GraphicsPipeline(Shader::ID shader_id, RenderPass::ID render_pass_id)
    : m_shader_id(shader_id),
      m_render_pass_id(render_pass_id),
      m_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
      m_vertex_attributes(MeshInputAttributeSet(
          MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT))
{
}

GraphicsPipeline::~GraphicsPipeline()
{
}

void GraphicsPipeline::AddSpatial(Spatial &&spatial)
{
    /* append any attributes not yet added */
    m_vertex_attributes.Merge(spatial.attributes);

    m_spatials.push_back(std::move(spatial));
}

void GraphicsPipeline::Create(Engine *engine)
{
    auto *shader = engine->GetShader(m_shader_id);
    AssertThrow(shader != nullptr);

    // TODO: Assert that render_pass matches the layout of what the fbo was set up with
    auto *render_pass = engine->GetRenderPass(m_render_pass_id);
    AssertThrow(render_pass != nullptr);

    renderer::GraphicsPipeline::ConstructionInfo construction_info{
        .vertex_attributes = m_vertex_attributes,
        .topology = m_topology,
        .cull_mode = renderer::GraphicsPipeline::CullMode::BACK,
        .depth_test = true,
        .depth_write = true,
        .shader = &shader->Get(),
        .render_pass = &render_pass->Get()
    };

    for (const auto &fbo_id : m_fbo_ids.ids) {
        if (auto *fbo = engine->GetFramebuffer(fbo_id)) {
            construction_info.fbos.push_back(&fbo->Get());
        }
    }

    EngineComponent::Create(engine, std::move(construction_info), &engine->GetInstance()->GetDescriptorPool());
}

void GraphicsPipeline::Destroy(Engine *engine)
{
    EngineComponent::Destroy(engine);
}

void GraphicsPipeline::Render(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index)
{
    auto *instance = engine->GetInstance();

    m_wrapped.BeginRenderPass(command_buffer, frame_index, VK_SUBPASS_CONTENTS_INLINE);
    m_wrapped.Bind(command_buffer);

    instance->GetDescriptorPool().BindDescriptorSets(command_buffer, &m_wrapped, 0, 3);

    /* TMP */
    for (auto &spatial : m_spatials) {
        m_wrapped.push_constants.material_index = spatial.material_id.value;
        m_wrapped.SubmitPushConstants(command_buffer);
        spatial.mesh->RenderVk(command_buffer, instance, nullptr);
    }

    m_wrapped.EndRenderPass(command_buffer, frame_index);
}

void GraphicsPipeline::Render(Engine *engine, CommandBuffer *primary_command_buffer, CommandBuffer *secondary_command_buffer, uint32_t frame_index)
{
    auto *instance = engine->GetInstance();

    m_wrapped.BeginRenderPass(primary_command_buffer, frame_index, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    secondary_command_buffer->Record(
        instance->GetDevice(),
        m_wrapped.GetConstructionInfo().render_pass,
        [this, instance, primary_command_buffer](CommandBuffer *secondary) {
            m_wrapped.Bind(secondary);

            instance->GetDescriptorPool().BindDescriptorSets(secondary, &m_wrapped, 0, 3);

            /* TMP */
            for (auto &spatial : m_spatials) {
                m_wrapped.push_constants.material_index = spatial.material_id.value;
                m_wrapped.SubmitPushConstants(secondary);
                spatial.mesh->RenderVk(secondary, instance, nullptr);
            }

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(primary_command_buffer);

    m_wrapped.EndRenderPass(primary_command_buffer, frame_index);
}

} // namespace hyperion::v2