#include "graphics.h"
#include "../engine.h"

#include <rendering/backend/renderer_descriptor_set.h>

#include "rendering/backend/renderer_graphics_pipeline.h"

namespace hyperion::v2 {

using renderer::MeshInputAttribute;

GraphicsPipeline::GraphicsPipeline(Ref<Shader> &&shader, Ref<Scene> &&scene, Ref<RenderPass> &&render_pass, Bucket bucket)
    : EngineComponent(),
      m_shader(std::move(shader)),
      m_scene(std::move(scene)),
      m_render_pass(std::move(render_pass)),
      m_bucket(bucket),
      m_topology(Topology::TRIANGLES),
      m_cull_mode(CullMode::BACK),
      m_fill_mode(FillMode::FILL),
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
      m_per_frame_data(nullptr)
{
}

GraphicsPipeline::~GraphicsPipeline()
{
    AssertThrowMsg(m_per_frame_data == nullptr, "per-frame data should have been destroyed");
}

void GraphicsPipeline::AddSpatial(Ref<Spatial> &&spatial)
{
    AssertThrow(spatial != nullptr);

    /* append any attributes not yet added */
    m_vertex_attributes.Merge(spatial->GetVertexAttributes());

    spatial.Init();
    spatial->OnAddedToPipeline(this);

    m_spatials.push_back(std::move(spatial));
}

void GraphicsPipeline::RemoveSpatial(Spatial::ID id)
{
    const auto it = std::find_if(
        m_spatials.begin(),
        m_spatials.end(),
        [&id](const auto &item) { return item->GetId() == id; }
    );

    if (it != m_spatials.end()) {
        auto &spatial = *it;

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

    if (m_scene != nullptr) {
        m_scene->Init(engine);
    }

    renderer::GraphicsPipeline::ConstructionInfo construction_info{
        .vertex_attributes = m_vertex_attributes,
        .topology          = m_topology,
        .cull_mode         = m_cull_mode,
        .fill_mode         = m_fill_mode,
        .depth_test        = m_depth_test,
        .depth_write       = m_depth_write,
        .blend_enabled     = m_blend_enabled,
        .shader            = &m_shader->Get(),
        .render_pass       = &m_render_pass->Get()
    };

    for (auto &fbo : m_fbos) {
        fbo.Init();
        construction_info.fbos.push_back(&fbo->Get());
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

    for (auto &spatial : m_spatials) {
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
                {{
                    .set = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,
                    .count = 1
                }}
            );

            static_assert(std::size(DescriptorSet::object_buffer_mapping) == Swapchain::max_frames_in_flight);

            const auto scene_index = m_scene != nullptr
                ? m_scene->GetId().value - 1
                : 0;

            /* Bind scene data - */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                &m_wrapped,
                {
                    {.set = DescriptorSet::scene_buffer_mapping[frame_index], .count = 1},
                    {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE},
                    {.offsets = {uint32_t(scene_index * sizeof(SceneShaderData))}}
                }
            );

            /* Bindless textures */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                &m_wrapped,
                {
                    {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
                    {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
                }
            );
            
            for (auto &spatial : m_spatials) {
                if (spatial->GetMesh() == nullptr) {
                    continue;
                }

                if (m_scene != nullptr && m_bucket != BUCKET_SKYBOX) {
                    auto &visibility_state = spatial->GetVisibilityState();

                    if (!visibility_state.Get(m_scene->GetId())) {
                        continue;
                    }
                }

                const auto spatial_index = spatial->GetId().value - 1;

                const auto material_index = spatial->GetMaterial() != nullptr
                    ? spatial->GetMaterial()->GetId().value - 1
                    : 0;

                const auto skeleton_index = spatial->GetSkeleton() != nullptr
                    ? spatial->GetSkeleton()->GetId().value - 1
                    : 0;

                /* Bind per-object / material data separately */
                instance->GetDescriptorPool().Bind(
                    device,
                    secondary,
                    &m_wrapped,
                    {
                        {.set = DescriptorSet::object_buffer_mapping[frame_index], .count = 1},
                        {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT},
                        {.offsets = {
                            uint32_t(material_index * sizeof(MaterialShaderData)),
                            uint32_t(spatial_index * sizeof(ObjectShaderData)),
                            uint32_t(skeleton_index * sizeof(SkeletonShaderData))
                        }}
                    }
                );

                spatial->GetMesh()->Render(engine, secondary);

                if (m_scene != nullptr) {
                    if (auto *octree = spatial->GetOctree()) {
                        octree->GetVisibilityState().Set(m_scene->GetId(), false);
                    }
                }
            }

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(primary);
}

} // namespace hyperion::v2