#include <rendering/debug/DebugDrawer.hpp>
#include <util/MeshBuilder.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::DynamicStorageBufferDescriptor;

DebugDrawer::DebugDrawer()
{
    m_draw_commands.Reserve(256);
}

DebugDrawer::~DebugDrawer()
{
}

void DebugDrawer::Create()
{
    m_shapes[uint(DebugDrawShape::SPHERE)] = MeshBuilder::NormalizedCubeSphere(8);
    m_shapes[uint(DebugDrawShape::BOX)] = MeshBuilder::Cube();
    m_shapes[uint(DebugDrawShape::PLANE)] = MeshBuilder::Quad();

    for (auto &shape : m_shapes) {
        AssertThrow(InitObject(shape));
    }

    m_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(DebugAABB),
        ShaderProperties(
            renderer::static_mesh_vertex_attributes,
            Array<String> { "IMMEDIATE_MODE" }
        )
    );

    AssertThrow(InitObject(m_shader));

    renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader().GetDefinition().GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);
    AssertThrow(descriptor_table != nullptr);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSet2Ref &debug_drawer_descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(DebugDrawerDescriptorSet), frame_index);
        AssertThrow(debug_drawer_descriptor_set != nullptr);

        debug_drawer_descriptor_set->SetElement(HYP_NAME(ImmediateDrawsBuffer), g_engine->GetRenderData()->immediate_draws.GetBuffer());
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_renderer_instance = g_engine->CreateRenderGroup(
        m_shader,
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_TRANSLUCENT,
                .fill_mode = FillMode::FILL,
                .blend_mode = BlendMode::NONE,
                .cull_faces = FaceCullMode::NONE
            }
        ),
        std::move(descriptor_table)
    );
    
    if (m_renderer_instance) {
        g_engine->GetDeferredSystem()
            .Get(m_renderer_instance->GetRenderableAttributes().GetMaterialAttributes().bucket)
            .AddFramebuffersToRenderGroup(m_renderer_instance);

        g_engine->InitObject(m_renderer_instance);
    }
}

void DebugDrawer::Destroy()
{
    m_shapes = { };

    m_renderer_instance.Reset();
    m_shader.Reset();
    
    SafeRelease(std::move(m_descriptor_sets));
}

void DebugDrawer::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (m_num_draw_commands_pending_addition.load(std::memory_order_relaxed) != 0) {
        UpdateDrawCommands();
    }

    if (m_draw_commands.Empty()) {
        return;
    }

    if (!m_renderer_instance) {
        m_draw_commands.Clear();

        return;
    }

    const uint frame_index = frame->GetFrameIndex();

    for (SizeType index = 0; index < m_draw_commands.Size(); index++) {
        const auto &draw_command = m_draw_commands[index];

        const ImmediateDrawShaderData shader_data {
            draw_command.transform_matrix,
            draw_command.color.Packed()
        };

        g_engine->GetRenderData()->immediate_draws.Set(index, shader_data);
    }

    g_engine->GetRenderData()->immediate_draws.UpdateBuffer(
        g_engine->GetGPUDevice(),
        frame->GetFrameIndex()
    );

    RendererProxy proxy = m_renderer_instance->GetProxy();
    proxy.Bind(frame);

    const uint debug_drawer_descriptor_set_index = proxy.GetGraphicsPipeline()->GetDescriptorTable().Get()->GetDescriptorSetIndex(HYP_NAME(DebugDrawerDescriptorSet));

    proxy.GetGraphicsPipeline()->GetDescriptorTable().Get()->Bind<GraphicsPipelineRef>(
        proxy.GetCommandBuffer(frame_index),
        frame_index,
        proxy.GetGraphicsPipeline(),
        {
            {
                HYP_NAME(DebugDrawerDescriptorSet),
                {
                    { HYP_NAME(ImmediateDrawsBuffer), HYP_RENDER_OBJECT_OFFSET(ImmediateDraw, 0) }
                }
            },
            {
                HYP_NAME(Scene),
                {
                    { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                    { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                    { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                    { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                    { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                }
            }
        }
    );

    for (SizeType index = 0; index < m_draw_commands.Size(); index++) {
        const DebugDrawCommand &draw_command = m_draw_commands[index];

        proxy.GetGraphicsPipeline()->GetDescriptorTable().Get()->GetDescriptorSet(HYP_NAME(DebugDrawerDescriptorSet), frame_index)->Bind(
            proxy.GetCommandBuffer(frame_index),
            proxy.GetGraphicsPipeline(),
            {
                { HYP_NAME(ImmediateDrawsBuffer), HYP_RENDER_OBJECT_OFFSET(ImmediateDraw, index) }
            },
            debug_drawer_descriptor_set_index
        );

        proxy.DrawMesh(frame, m_shapes[uint(draw_command.shape)].Get());
    }

    proxy.Submit(frame);

    m_draw_commands.Clear();
}

void DebugDrawer::UpdateDrawCommands()
{
    std::lock_guard guard(m_draw_commands_mutex);

    SizeType size = m_draw_commands_pending_addition.Size();
    int64 previous_value = m_num_draw_commands_pending_addition.fetch_sub(int64(size), std::memory_order_relaxed);
    AssertThrow(previous_value - int64(size) >= 0);

    m_draw_commands.Concat(std::move(m_draw_commands_pending_addition));
}

void DebugDrawer::CommitCommands(DebugDrawCommandList &&command_list)
{
    std::lock_guard guard(m_draw_commands_mutex);
    
    m_num_draw_commands_pending_addition.fetch_add(int64(command_list.m_draw_commands.Size()), std::memory_order_relaxed);
    m_draw_commands_pending_addition.Concat(std::move(command_list.m_draw_commands));
}

void DebugDrawer::Sphere(const Vector3 &position, float radius, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        Transform(position, radius, Quaternion::identity).GetMatrix(),
        color
    });
}

void DebugDrawer::Box(const Vector3 &position, const Vector3 &size, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::BOX,
        Transform(position, size, Quaternion::identity).GetMatrix(),
        color
    });
}

void DebugDrawer::Plane(const FixedArray<Vector3, 4> &points, Color color)
{
    Vector3 x = (points[1] - points[0]).Normalize();
    Vector3 y = (points[2] - points[0]).Normalize();
    Vector3 z = x.Cross(y).Normalize();

    const Vector3 center = points.Avg();

    Matrix4 transform_matrix;
    transform_matrix.rows[0] = Vector4(x, 0.0f);
    transform_matrix.rows[1] = Vector4(y, 0.0f);
    transform_matrix.rows[2] = Vector4(z, 0.0f);
    transform_matrix.rows[3] = Vector4(center, 1.0f);

    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::PLANE,
        transform_matrix,
        color
    });
}

// command list methods

void DebugDrawCommandList::Sphere(const Vector3 &position, float radius, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::SPHERE,
        Transform(position, radius, Quaternion::identity).GetMatrix(),
        color
    });
}

void DebugDrawCommandList::Box(const Vector3 &position, const Vector3 &size, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::BOX,
        Transform(position, size, Quaternion::identity).GetMatrix(),
        color
    });
}

void DebugDrawCommandList::Plane(const Vector3 &position, const Vector2 &size, Color color)
{
    m_draw_commands.PushBack(DebugDrawCommand {
        DebugDrawShape::PLANE,
        Transform(position, Vector3(size.x, size.y, 1.0f), Quaternion::identity).GetMatrix(),
        color
    });
}

void DebugDrawCommandList::Commit()
{
    m_debug_drawer->CommitCommands(std::move(*this));
}

} // namespace hyperion::v2