#include "graphics.h"
#include "../engine.h"

#include <rendering/backend/renderer_descriptor_set.h>

#include "rendering/backend/renderer_graphics_pipeline.h"

namespace hyperion::v2 {

using renderer::MeshInputAttribute;

GraphicsPipeline::GraphicsPipeline(Ref<Shader> &&shader, RenderPass::ID render_pass_id, Bucket bucket)
    : EngineComponent(),
      m_shader(std::move(shader)),
      m_render_pass_id(render_pass_id),
      m_bucket(bucket),
      m_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
      m_cull_mode(renderer::GraphicsPipeline::CullMode::BACK),
      m_fill_mode(renderer::GraphicsPipeline::FillMode::FILL),
      m_depth_test(true),
      m_depth_write(true),
      m_blend_enabled(false),
      m_vertex_attributes(MeshInputAttributeSet(
          MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT)),
      m_per_frame_data(nullptr),
      m_scene_index(0)
{
}

GraphicsPipeline::~GraphicsPipeline()
{
    AssertThrowMsg(m_per_frame_data == nullptr, "per-frame data should have been destroyed");
}

void GraphicsPipeline::AddSpatial(Engine *engine, Spatial *spatial)
{
    AssertThrow(spatial != nullptr);

    /* append any attributes not yet added */
    m_vertex_attributes.Merge(spatial->GetVertexAttributes());

    /* Update object data */
    engine->m_shader_globals->objects.Set(spatial->GetId().Value() - 1, {.model_matrix = spatial->GetTransform().GetMatrix()});

    spatial->OnAddedToPipeline(this);

    m_spatials.push_back(spatial);
}

void GraphicsPipeline::RemoveSpatial(Engine *engine, Spatial *spatial)
{
    const auto it = std::find(m_spatials.begin(), m_spatials.end(), spatial);

    if (it != m_spatials.end()) {
        if (spatial != nullptr) {
            spatial->OnRemovedFromPipeline(this);
        }

        m_spatials.erase(it);
    }
}

void GraphicsPipeline::OnSpatialRemoved(Spatial *spatial)
{
    const auto it = std::find(m_spatials.begin(), m_spatials.end(), spatial);
    AssertThrow(it != m_spatials.end());

    m_spatials.erase(it);
}

void GraphicsPipeline::Create(Engine *engine)
{
    AssertThrow(m_shader != nullptr);
    m_shader->Init(engine);
    
    auto *render_pass = engine->resources.render_passes[m_render_pass_id];
    AssertThrow(render_pass != nullptr);

    renderer::GraphicsPipeline::ConstructionInfo construction_info{
        .vertex_attributes = m_vertex_attributes,
        .topology          = m_topology,
        .cull_mode         = m_cull_mode,
        .fill_mode         = m_fill_mode,
        .depth_test        = m_depth_test,
        .depth_write       = m_depth_write,
        .blend_enabled     = m_blend_enabled,
        .shader            = &m_shader->Get(),
        .render_pass       = &render_pass->Get()
    };

    for (const auto &fbo_id : m_fbo_ids.ids) {
        if (auto *fbo = engine->resources.framebuffers[fbo_id]) {
            construction_info.fbos.push_back(&fbo->Get());
        }
    }

    AssertThrow(m_per_frame_data == nullptr);
    m_per_frame_data = new PerFrameData<CommandBuffer>(engine->GetInstance()->GetFrameHandler()->NumFrames());

    auto &per_frame_data = *m_per_frame_data;

    for (uint32_t i = 0; i < per_frame_data.NumFrames(); i++) {
        auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_SECONDARY);
        HYPERION_ASSERT_RESULT(command_buffer->Create(
            engine->GetInstance()->GetDevice(),
            engine->GetInstance()->GetGraphicsCommandPool()
        ));

        per_frame_data[i].Set<CommandBuffer>(std::move(command_buffer));
    }

    EngineComponent::Create(engine, std::move(construction_info), &engine->GetInstance()->GetDescriptorPool());
}

void GraphicsPipeline::Destroy(Engine *engine)
{
    if (m_per_frame_data != nullptr) {
        auto &per_frame_data = *m_per_frame_data;

        for (uint32_t i = 0; i < per_frame_data.NumFrames(); i++) {
            HYPERION_ASSERT_RESULT(per_frame_data[i].Get<CommandBuffer>()->Destroy(
                engine->GetInstance()->GetDevice(),
                engine->GetInstance()->GetGraphicsCommandPool()
            ));
        }

        delete m_per_frame_data;
        m_per_frame_data = nullptr;
    }

    for (auto *spatial : m_spatials) {
        if (spatial == nullptr) {
            continue;
        }

        spatial->OnRemovedFromPipeline(this);
    }

    m_spatials.clear();

    EngineComponent::Destroy(engine);
}

void GraphicsPipeline::Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index)
{
    auto *instance = engine->GetInstance();
    auto *device = instance->GetDevice();
    auto *secondary_command_buffer = m_per_frame_data->At(frame_index).Get<CommandBuffer>();

    secondary_command_buffer->Record(
        device,
        m_wrapped.GetConstructionInfo().render_pass,
        [this, engine, instance, device, frame_index](CommandBuffer *secondary) {
            m_wrapped.Bind(secondary);

            /* Bind global data */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                &m_wrapped,
                {{.set = 0, .count = 2}}
            );

            static constexpr uint32_t frame_index_scene_buffer_mapping[]  = {2, 4};
            static constexpr uint32_t frame_index_object_buffer_mapping[] = {3, 5};
            static constexpr uint32_t frame_index_bindless_textures_mapping[] = { DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS, DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1 };

            static_assert(std::size(frame_index_object_buffer_mapping) == renderer::Swapchain::max_frames_in_flight);

            /* Bind scene data - */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                &m_wrapped,
                {
                    {.set = frame_index_scene_buffer_mapping[frame_index], .count = 1},
                    {.binding = 2},
                    {.offsets = {uint32_t(m_scene_index * sizeof(SceneShaderData))}}
                }
            );

            /* Bindless textures */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                &m_wrapped,
                {
                    {.set = frame_index_bindless_textures_mapping[frame_index], .count = 1},
                    {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
                }
            );
            
            for (const Spatial *spatial : m_spatials) {
                if (spatial->GetMesh() == nullptr) {
                    continue;
                }

                const auto spatial_index = spatial->GetId().value - 1;

                const auto material_index = spatial->GetMaterial() != nullptr
                    ? spatial->GetMaterial()->GetId().value - 1
                    : 0;

                /* Bind per-object / material data separately */
                instance->GetDescriptorPool().Bind(
                    device,
                    secondary,
                    &m_wrapped,
                    {
                        {.set = frame_index_object_buffer_mapping[frame_index], .count = 1},
                        {.binding = 3},
                        {.offsets = {
                            uint32_t(material_index * sizeof(MaterialShaderData)),
                            uint32_t(spatial_index * sizeof(ObjectShaderData))
                        }}
                    }
                );

                spatial->GetMesh()->Render(engine, secondary);
            }

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(primary);
}

} // namespace hyperion::v2