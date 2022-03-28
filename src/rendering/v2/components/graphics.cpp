#include "graphics.h"
#include "../engine.h"

#include <rendering/backend/renderer_descriptor_set.h>

#include "rendering/backend/renderer_graphics_pipeline.h"

namespace hyperion::v2 {

using renderer::MeshInputAttribute;

GraphicsPipeline::GraphicsPipeline(Shader::ID shader_id, RenderPass::ID render_pass_id, Bucket bucket)
    : m_shader_id(shader_id),
      m_render_pass_id(render_pass_id),
      m_bucket(bucket),
      m_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
      m_cull_mode(renderer::GraphicsPipeline::CullMode::BACK),
      m_fill_mode(renderer::GraphicsPipeline::FillMode::FILL),
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

void GraphicsPipeline::AddSpatial(Engine *engine, Spatial::ID id)
{
    Spatial *spatial = engine->GetSpatial(id);

    AssertThrow(spatial != nullptr);

    /* append any attributes not yet added */
    m_vertex_attributes.Merge(spatial->GetVertexAttributes());

    /* Update object data */
    engine->m_shader_globals->objects.Set(id.value - 1, {.model_matrix = spatial->GetTransform().GetMatrix()});

    spatial->OnAddedToPipeline(this);

    m_spatials.emplace_back(id, spatial);
}

void GraphicsPipeline::RemoveSpatial(Engine *engine, Spatial::ID id)
{
    const auto it = std::find_if(m_spatials.begin(), m_spatials.end(), [id](const auto &it) {
        return it.first == id;
    });

    if (it != m_spatials.end()) {
        it->second->OnRemovedFromPipeline(this);

        m_spatials.erase(it);
    }
}

void GraphicsPipeline::OnSpatialRemoved(Spatial *spatial)
{
    const auto it = std::find_if(m_spatials.begin(), m_spatials.end(), [spatial](const auto &it) {
        return it.second == spatial;
    });

    if (it != m_spatials.end()) {
        m_spatials.erase(it);
    }
}

void GraphicsPipeline::SetSpatialTransform(Engine *engine, uint32_t index, const Transform &transform)
{
    const auto &item = m_spatials[index];

    Spatial *spatial = item.second;
    spatial->SetTransform(transform);

    engine->m_shader_globals->objects.Set(item.first.value - 1, {.model_matrix = spatial->GetTransform().GetMatrix()});
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
        .cull_mode = m_cull_mode,
        .fill_mode = m_fill_mode,
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
    for (auto &it : m_spatials) {
        it.second->OnRemovedFromPipeline(this);
    }

    m_spatials.clear();

    EngineComponent::Destroy(engine);
}

void GraphicsPipeline::Render(Engine *engine, CommandBuffer *primary_command_buffer, CommandBuffer *secondary_command_buffer, uint32_t frame_index)
{
    auto *instance = engine->GetInstance();

    m_wrapped.BeginRenderPass(primary_command_buffer, frame_index, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    secondary_command_buffer->Record(
        instance->GetDevice(),
        m_wrapped.GetConstructionInfo().render_pass,
        [this, instance, frame_index](CommandBuffer *secondary) {
            m_wrapped.Bind(secondary);

            /* Bind global data */
            instance->GetDescriptorPool().Bind(
                secondary,
                &m_wrapped,
                {{.set = 0, .count = 2}}
            );

            static constexpr uint32_t frame_index_scene_buffer_mapping[]  = {2, 4};
            static constexpr uint32_t frame_index_object_buffer_mapping[] = {3, 5};

            /* Bind scene data - */
            instance->GetDescriptorPool().Bind(
                secondary,
                &m_wrapped,
                {
                    {.set = frame_index_scene_buffer_mapping[frame_index], .count = 1},
                    {.binding = 2}
                }
            );
            
            for (const auto &spatial : m_spatials) {
                /* Bind per-object / material data separately */
                instance->GetDescriptorPool().Bind(
                    secondary,
                    &m_wrapped,
                    {
                        {.set = frame_index_object_buffer_mapping[frame_index], .count = 1},
                        {.binding = 3},
                        {.offsets = {
                            uint32_t((spatial.second->GetMaterialId().value - 1) * sizeof(MaterialShaderData)),
                            uint32_t((spatial.first.value - 1) * sizeof(ObjectShaderData))
                        }}
                    }
                );

                spatial.second->GetMesh()->RenderVk(secondary, instance, nullptr);
            }

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(primary_command_buffer);

    m_wrapped.EndRenderPass(primary_command_buffer, frame_index);
}

} // namespace hyperion::v2