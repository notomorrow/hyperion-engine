#include "graphics.h"
#include "../engine.h"

#include <rendering/backend/renderer_descriptor_set.h>

#include "rendering/backend/renderer_graphics_pipeline.h"

namespace hyperion::v2 {

bool GraphicsPipeline::BucketSupportsCulling(Bucket bucket)
{
    return bucket != BUCKET_SKYBOX
        && bucket != BUCKET_VOXELIZER;
}

GraphicsPipeline::GraphicsPipeline(
    Ref<Shader> &&shader,
    Ref<RenderPass> &&render_pass,
    const VertexAttributeSet &vertex_attributes,
    Bucket bucket
) : EngineComponent(),
    m_shader(std::move(shader)),
    m_render_pass(std::move(render_pass)),
    m_bucket(bucket),
    m_topology(Topology::TRIANGLES),
    m_cull_mode(CullMode::BACK),
    m_fill_mode(FillMode::FILL),
    m_depth_test(true),
    m_depth_write(true),
    m_blend_enabled(false),
    m_vertex_attributes(vertex_attributes),
    m_per_frame_data(nullptr)
{
}

GraphicsPipeline::~GraphicsPipeline()
{
    Teardown();
}

void GraphicsPipeline::AddSpatial(Ref<Spatial> &&spatial)
{
    AssertThrow(spatial != nullptr);
    AssertThrowMsg(
        m_vertex_attributes & spatial->GetVertexAttributes() == spatial->GetVertexAttributes(),
        "Pipeline vertex attributes does not satisfy the required vertex attributes of the spatial."
    );

    spatial.Init();
    spatial->OnAddedToPipeline(this);
    
    std::lock_guard guard(m_enqueued_spatials_mutex);

    m_spatials_pending_addition.push_back(std::move(spatial));
}

bool GraphicsPipeline::RemoveFromSpatialList(
    Spatial::ID id,
    std::vector<Ref<Spatial>> &spatials,
    bool call_on_removed,
    bool remove_immediately
)
{
    const auto it = std::find_if(
        spatials.begin(),
        spatials.end(),
        [&id](const auto &item) {
            return item->GetId() == id;
        }
    );

    if (it == spatials.end()) {
        return false;
    }

    auto &found_spatial = *it;

    if (call_on_removed && found_spatial != nullptr) {
        found_spatial->OnRemovedFromPipeline(this);
    }

    if (remove_immediately) {
        spatials.erase(it);
    } else {
        std::lock_guard guard(m_enqueued_spatials_mutex);

        m_spatials_pending_removal.push_back(it->Acquire());
    }
    
    return true;
}

void GraphicsPipeline::RemoveSpatial(Spatial::ID id)
{
    if (RemoveFromSpatialList(id, m_spatials, true, false)) {
        return;
    }

    std::lock_guard guard(m_enqueued_spatials_mutex);

    if (RemoveFromSpatialList(id, m_spatials_pending_addition, true, true)) {
        return;
    }

    RemoveFromSpatialList(id, m_spatials_pending_removal, false, true);
}

void GraphicsPipeline::OnSpatialRemoved(Spatial *spatial)
{
    const auto id = spatial->GetId();

    if (RemoveFromSpatialList(id, m_spatials, false, false)) {
        return;
    }

    std::lock_guard guard(m_enqueued_spatials_mutex);

    if (RemoveFromSpatialList(id, m_spatials_pending_addition, true, true)) {
        return;
    }

    RemoveFromSpatialList(id, m_spatials_pending_removal, false, true);
}

void GraphicsPipeline::PerformEnqueuedSpatialUpdates(Engine *engine)
{
    std::lock_guard guard(m_enqueued_spatials_mutex);

    if (!m_spatials_pending_removal.empty()) {
        for (auto &spatial : m_spatials_pending_removal) {
            const auto it = std::find(m_spatials.begin(), m_spatials.end(), spatial.ptr); // oof

            if (it != m_spatials.end()) {
                m_spatials.erase(it);
            }
        }

        m_spatials_pending_removal.clear();
    }

    if (!m_spatials_pending_addition.empty()) {
        m_spatials.insert(
            m_spatials.end(),
            std::make_move_iterator(m_spatials_pending_addition.begin()), 
            std::make_move_iterator(m_spatials_pending_addition.end())
        );

        m_spatials_pending_addition.clear();
    }
}

void GraphicsPipeline::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](Engine *engine) {
        AssertThrow(m_shader != nullptr);
        m_shader.Init();

        renderer::GraphicsPipeline::ConstructionInfo construction_info{
            .vertex_attributes = m_vertex_attributes,
            .topology          = m_topology,
            .cull_mode         = m_cull_mode,
            .fill_mode         = m_fill_mode,
            .depth_test        = m_depth_test,
            .depth_write       = m_depth_write,
            .blend_enabled     = m_blend_enabled,
            .shader            = m_shader->GetShaderProgram(),
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

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_GRAPHICS_PIPELINES, [this](Engine *engine) {
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
            
            std::lock_guard guard(m_enqueued_spatials_mutex);

            for (auto &spatial : m_spatials_pending_addition) {
                if (spatial == nullptr) {
                    continue;
                }

                spatial->OnRemovedFromPipeline(this);
            }

            m_spatials_pending_addition.clear();
            m_spatials_pending_removal.clear();

            EngineComponent::Destroy(engine);
        }), engine);
    }));
}

void GraphicsPipeline::Render(
    Engine *engine,
    CommandBuffer *primary,
    uint32_t frame_index
)
{
    AssertThrow(m_per_frame_data != nullptr);

    if (!m_spatials_pending_addition.empty() || !m_spatials_pending_removal.empty()) {
        PerformEnqueuedSpatialUpdates(engine);
    }

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

            const auto scene_id = engine->render_bindings.GetScene();

            const auto scene_index = scene_id != Scene::bad_id
                ? scene_id.value - 1
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

            const bool perform_culling = scene_id != Scene::bad_id && BucketSupportsCulling(m_bucket);
            
            for (auto &spatial : m_spatials) {
                if (spatial->GetMesh() == nullptr) {
                    continue;
                }

                if (perform_culling) {
                    if (auto *octant = spatial->GetOctree()) {
                        const auto &visibility_state = octant->GetVisibilityState();

                        if (!Octree::IsVisible(&engine->GetOctree(), octant)) {
                            continue;
                        }

                        if (!visibility_state.Get(scene_id)) {
                            continue;
                        }
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
            }

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(primary);
}

} // namespace hyperion::v2