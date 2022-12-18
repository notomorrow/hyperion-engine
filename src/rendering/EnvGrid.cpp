#include <rendering/EnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <scene/controllers/PagingController.hpp>
#include <scene/Scene.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Image;
using renderer::Result;

class EnvGrid;

class EnvGridPagingController : public PagingController
{
public:
    EnvGridPagingController(
        EnvGrid *env_grid,
        Extent3D patch_size,
        const Vector3 &scale,
        Float max_distance
    );
    virtual ~EnvGridPagingController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;

protected:
    virtual void OnPatchAdded(Patch *patch) override;
    virtual void OnPatchRemoved(Patch *patch) override;

private:
    EnvGrid *m_env_grid;
};

EnvGridPagingController::EnvGridPagingController(
    EnvGrid *env_grid,
    Extent3D patch_size,
    const Vector3 &scale,
    Float max_distance
) : PagingController("EnvGridPagingController", patch_size, scale, max_distance),
    m_env_grid(env_grid)
{
}

void EnvGridPagingController::OnAdded()
{
    AssertThrow(m_env_grid != nullptr);

    PagingController::OnAdded();
}

void EnvGridPagingController::OnRemoved()
{
    PagingController::OnRemoved();
}

void EnvGridPagingController::OnPatchAdded(Patch *patch)
{
    DebugLog(LogType::Info, "EnvGrid added %f, %f\n", patch->info.coord.x, patch->info.coord.y);
}

void EnvGridPagingController::OnPatchRemoved(Patch *patch)
{
    DebugLog(LogType::Info, "EnvGrid removed %f, %f\n", patch->info.coord.x, patch->info.coord.y);
}

const Extent2D EnvGrid::cubemap_dimensions = Extent2D { 128, 128 };
const Float EnvGrid::overlap_amount = 10.0f;

EnvGrid::EnvGrid(const BoundingBox &aabb, const Extent3D &density)
    : RenderComponent(2),
      m_aabb(aabb),
      m_density(density),
      m_current_probe_index(0)
{
}

EnvGrid::~EnvGrid()
{
}

void EnvGrid::Init()
{
    Handle<Scene> scene(GetParent()->GetScene()->GetID());
    AssertThrow(scene.IsValid());

    const SizeType total_count = m_density.Size();
    const Vector3 aabb_extent = m_aabb.GetExtent();

    m_env_probes.Resize(total_count);

    for (SizeType x = 0; x < m_density.width; x++) {
        for (SizeType y = 0; y < m_density.height; y++) {
            for (SizeType z = 0; z < m_density.depth; z++) {
                const SizeType index = x * m_density.height * m_density.depth
                     + y * m_density.depth
                     + z;

                AssertThrow(!m_env_probes[index].IsValid());

                const BoundingBox env_probe_aabb(
                    m_aabb.min + (Vector3(Float(x), Float(y), Float(z)) * (aabb_extent / Vector3(m_density))) - Vector3(overlap_amount),
                    m_aabb.min + (Vector3(Float(x + 1), Float(y + 1), Float(z + 1)) * (aabb_extent / Vector3(m_density))) + Vector3(overlap_amount)
                );

                m_env_probes[index] = CreateObject<EnvProbe>(scene, env_probe_aabb);
            }
        }
    }

    CreateShader();
    CreateFramebuffer();

    {
        Memory::Set(m_shader_data.probe_indices, 0, sizeof(m_shader_data.probe_indices));
        m_shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
        m_shader_data.extent = Vector4(m_aabb.GetExtent(), 1.0f);
        m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };
        m_shader_data.enabled_indices_mask = 0u;
    }

    m_scene = CreateObject<Scene>(CreateObject<Camera>(
        90.0f,
        -cubemap_dimensions.width, cubemap_dimensions.height,
        0.01f, ((aabb_extent / Vector3(m_density)) * 0.5f).Max() + overlap_amount
    ));

    m_scene->GetCamera()->SetFramebuffer(m_framebuffer);
    m_scene->SetName("EnvGrid Scene");
    m_scene->SetOverrideRenderableAttributes(RenderableAttributeSet(
        MeshAttributes { },
        MaterialAttributes {
            .bucket = BUCKET_INTERNAL,
            .cull_faces = FaceCullMode::FRONT
        },
        m_shader->GetID()
    ));
    m_scene->SetParentScene(Handle<Scene>(GetParent()->GetScene()->GetID()));

    InitObject(m_scene);
    Engine::Get()->GetWorld()->AddScene(m_scene);

    for (auto &env_probe : m_env_probes) {
        if (InitObject(env_probe)) {
            env_probe->EnqueueBind();
        }
    }

    DebugLog(LogType::Info, "Created %llu total EnvProbes in grid\n", total_count);
}

// called from game thread
void EnvGrid::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);

    m_entity = CreateObject<Entity>();
    m_entity->SetName("EnvGrid Entity");
    m_entity->AddController<EnvGridPagingController>(
        this,
        Extent3D(m_aabb.GetExtent() / Vector3(m_density)),
        Vector3::one,
        1.0f
    );

    GetParent()->GetScene()->AddEntity(m_entity);
}

void EnvGrid::OnRemoved()
{
    if (m_entity) {
        m_entity->RemoveController<EnvGridPagingController>();

        GetParent()->GetScene()->RemoveEntity(m_entity);
        m_entity.Reset();
    }

    for (auto &env_probe : m_env_probes) {
        env_probe->EnqueueUnbind();
    }

    Engine::Get()->GetWorld()->RemoveScene(m_scene->GetID());
    m_scene.Reset();

    Engine::Get()->SafeReleaseHandle<Shader>(std::move(m_shader));

    struct RENDER_COMMAND(DestroyEnvGridFramebufferAttachments) : RenderCommand
    {
        EnvGrid &env_grid;

        RENDER_COMMAND(DestroyEnvGridFramebufferAttachments)(EnvGrid &env_grid)
            : env_grid(env_grid)
        {
        }

        virtual Result operator()()
        {
            auto result = Result::OK;

            if (env_grid.m_framebuffer != nullptr) {
                for (auto &attachment : env_grid.m_attachments) {
                    env_grid.m_framebuffer->RemoveAttachmentRef(attachment.get());
                }
            }

            for (auto &attachment : env_grid.m_attachments) {
                HYPERION_PASS_ERRORS(attachment->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()), result);
            }

            env_grid.m_attachments.clear();

            return result;
        }
    };

    PUSH_RENDER_COMMAND(DestroyEnvGridFramebufferAttachments, *this);
}

void EnvGrid::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    for (auto &env_probe : m_env_probes) {
        // TODO: Just update render data in Render()
        env_probe->Update();
    }
}

void EnvGrid::OnRender(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    if (m_env_probes.Empty()) {
        return;
    }

    const UInt num_env_probes = UInt(m_env_probes.Size());

    m_shader_data.enabled_indices_mask = 0u;

    const Vector3 &camera_position = Engine::Get()->GetRenderState().GetScene().scene.camera.position;
    Int bound_probe_index = 0;

    for (UInt index = 0; index < num_env_probes; index++) {
        auto &env_probe = m_env_probes[index];

        const BoundingBox &probe_aabb = env_probe->GetAABB();
        const Vector3 probe_center = probe_aabb.GetCenter();
        // const Vector3 probe_aabb_extent = probe_aabb.GetExtent();


        const Vector3 local_extent = m_aabb.GetExtent() / Vector3(m_density);

        const Vector3 diff = probe_center - m_aabb.GetCenter();//probe_center - camera_position;

        const Vector3 probe_position_clamped = ((diff + (m_aabb.GetExtent() * 0.5f)) / m_aabb.GetExtent());

        const Vector3 probe_diff_units = probe_position_clamped * Vector3(m_density);

        Int probe_index_at_point = (Int(probe_diff_units.x) * Int(m_density.height) * Int(m_density.depth))
            + (Int(probe_diff_units.y) * Int(m_density.depth))
            + Int(probe_diff_units.z);

        if (probe_index_at_point < 0 || probe_index_at_point >= max_bound_env_probes) {
            DebugLog(
                LogType::Warn,
                "Probe %u out of range of max bound env probes (index: %d, position in units: %f, %f, %f)\n",
                env_probe->GetID().Value(),
                probe_index_at_point,
                probe_diff_units.x,
                probe_diff_units.y,
                probe_diff_units.z
            );

            continue;
        }

        // TODO: only update if changed.
        env_probe->UpdateRenderData(probe_index_at_point);
        m_shader_data.probe_indices[probe_index_at_point] = env_probe->GetID().ToIndex();
        m_shader_data.enabled_indices_mask |= (1 << probe_index_at_point);

        if (index == m_current_probe_index) {
            bound_probe_index = probe_index_at_point;
        }
    }

    Handle<EnvProbe> &current_env_probe = m_env_probes[m_current_probe_index];

    { // update shader data
        m_shader_data.extent = Vector4(m_aabb.GetExtent(), 1.0f);
        m_shader_data.center = Vector4(m_aabb.GetCenter(), 1.0f);
        m_shader_data.density = { m_density.width, m_density.height, m_density.depth, 0 };

        Engine::Get()->GetRenderData()->env_grids.Set(GetComponentIndex(), m_shader_data);
    }

    auto *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    auto result = Result::OK;

    // Use EnqueueBind() and EnqueueUnbind() when loading in and unloading?

    struct alignas(128) { UInt32 env_probe_index; } push_constants;
    push_constants.env_probe_index = current_env_probe->GetID().ToIndex();
    m_scene->Render(frame, &push_constants, sizeof(push_constants));

    { // copy current framebuffer image to EnvProbe texture, generate mipmap
        Image *framebuffer_image = m_framebuffer->GetAttachmentRefs()[0]->GetAttachment()->GetImage();
        Handle<Texture> &current_cubemap_texture = current_env_probe->GetTexture();

        framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
        current_cubemap_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        HYPERION_PASS_ERRORS(
            current_cubemap_texture->GetImage()->Blit(command_buffer, framebuffer_image),
            result
        );

        HYPERION_PASS_ERRORS(
            current_cubemap_texture->GetImage()->GenerateMipmaps(Engine::Get()->GetGPUDevice(), command_buffer),
            result
        );

        framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        current_cubemap_texture->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

        HYPERION_ASSERT_RESULT(result);
    }

    m_current_probe_index = (m_current_probe_index + 1) % num_env_probes;
}

void EnvGrid::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

void EnvGrid::CreateShader()
{
    m_shader = CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader(
        "CubemapRenderer",
        ShaderProps({ "LIGHTING", "SHADOWS" })
    ));

    InitObject(m_shader);
}

void EnvGrid::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        cubemap_dimensions,
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER,
        6
    );

    AttachmentRef *attachment_ref;

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            cubemap_dimensions,
            InternalFormat::RGBA8_SRGB,
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    m_framebuffer->AddAttachmentRef(attachment_ref);

    m_attachments.push_back(std::make_unique<Attachment>(
        std::make_unique<renderer::FramebufferImageCube>(
            cubemap_dimensions,
            Engine::Get()->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.back()->AddAttachmentRef(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_ref
    ));

    m_framebuffer->AddAttachmentRef(attachment_ref);

    // attachment should be created in render thread?
    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(Engine::Get()->GetGPUInstance()->GetDevice()));
    }

    InitObject(m_framebuffer);
}

} // namespace hyperion::v2
