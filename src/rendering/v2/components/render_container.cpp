#include "render_container.h"
#include "../engine.h"

#include <rendering/backend/renderer_descriptor.h>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;

RenderContainer::RenderContainer(Shader::ID shader_id, RenderPass::ID render_pass_id)
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
    /* TMP */
}

RenderContainer::~RenderContainer()
{
}

void RenderContainer::AddSpatial(Spatial &&spatial)
{
    /* append any attributes not yet added */
    m_vertex_attributes.Merge(spatial.attributes);

    m_spatials.push_back(std::move(spatial));
}

void RenderContainer::Create(Engine *engine)
{
    auto *shader = engine->GetShader(m_shader_id);
    AssertThrow(shader != nullptr);

    // TODO: Assert that render_pass matches the layout of what the fbo was set up with
    auto *render_pass = engine->GetRenderPass(m_render_pass_id);
    AssertThrow(render_pass != nullptr);

    renderer::GraphicsPipeline::ConstructionInfo construction_info{
        .vertex_attributes = m_vertex_attributes,
        .topology = m_topology,
        .cull_mode = renderer::GraphicsPipeline::ConstructionInfo::CullMode::BACK,
        .depth_test = true,
        .depth_write = true,
        .shader = &shader->Get(),
        .render_pass = &render_pass->Get()
    };

    for (const auto &fbo_id : m_fbo_ids.ids) {
        if (auto *fbo = engine->GetFramebuffer(fbo_id)) {
            AssertThrow(fbo != nullptr);

            construction_info.fbos.push_back(&fbo->Get());
        }
    }

    EngineComponent::Create(engine, std::move(construction_info), &engine->GetInstance()->GetDescriptorPool());
}

void RenderContainer::Destroy(Engine *engine)
{
    EngineComponent::Destroy(engine);
}

void RenderContainer::Render(Engine *engine, CommandBuffer *command_buffer, uint32_t frame_index)
{
    auto *instance = engine->GetInstance();

    m_wrapped.BeginRenderPass(command_buffer, frame_index, VK_SUBPASS_CONTENTS_INLINE);
    m_wrapped.Bind(command_buffer);

    instance->GetDescriptorPool().BindDescriptorSets(command_buffer, &m_wrapped);

    /* TMP */
    for (auto &spatial : m_spatials) {
        m_wrapped.push_constants.material_index = spatial.material_id.value;
        m_wrapped.SubmitPushConstants(command_buffer);
        spatial.mesh->RenderVk(command_buffer, instance, nullptr);
    }

    m_wrapped.EndRenderPass(command_buffer, frame_index);
}

} // namespace hyperion::v2