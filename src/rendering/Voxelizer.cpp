#include "Voxelizer.hpp"
#include "../Engine.hpp"

#include <rendering/backend/RendererFeatures.hpp>

#include <math/MathUtil.hpp>
#include <asset/ByteReader.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::Result;

Voxelizer::Voxelizer()
    : EngineComponentBase(),
      m_num_fragments(0)
{
}

Voxelizer::~Voxelizer()
{
    Teardown();
}

void Voxelizer::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    const auto voxel_map_size_signed = static_cast<Int64>(voxel_map_size);

    m_scene = CreateObject<Scene>(CreateObject<Camera>(voxel_map_size, voxel_map_size));

    m_scene->GetCamera()->SetCameraController(UniquePtr<OrthoCameraController>::Construct(
        -voxel_map_size_signed / 2, voxel_map_size_signed / 2,
        -voxel_map_size_signed / 2, voxel_map_size_signed / 2,
        -voxel_map_size_signed / 2, voxel_map_size_signed / 2
    ));

    InitObject(m_scene);
    
    CreateBuffers();
    CreateShader();
    CreateRenderPass();
    CreateFramebuffer();
    CreateDescriptors();
    CreatePipeline();

    OnTeardown([this]() {
        m_shader.Reset();
        m_framebuffer.Reset();
        m_render_pass.Reset();

        struct RENDER_COMMAND(DestroyVoxelizer) : RenderCommand
        {
            Voxelizer &voxelizer;

            RENDER_COMMAND(DestroyVoxelizer)(Voxelizer &voxelizer)
                : voxelizer(voxelizer)
            {
            }

            virtual Result operator()()
            {
                auto result = renderer::Result::OK;

                if (voxelizer.m_counter != nullptr) {
                    voxelizer.m_counter->Destroy();
                }

                if (voxelizer.m_fragment_list_buffer != nullptr) {
                    Engine::Get()->SafeRelease(UniquePtr<StorageBuffer>(voxelizer.m_fragment_list_buffer.release()));
                }

                for (auto &attachment : voxelizer.m_attachments) {
                    HYPERION_PASS_ERRORS(
                        attachment->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()),
                        result
                    );
                }

                voxelizer.m_num_fragments = 0;

                return result;
            }
        };

        RenderCommands::Push<RENDER_COMMAND(DestroyVoxelizer)>(*this);

        HYP_SYNC_RENDER();

        m_renderer_instance.Reset();
        m_attachments.clear();

        m_fragment_list_buffer.reset();
        m_counter.reset();
    });
}

void Voxelizer::CreatePipeline()
{
    m_renderer_instance = CreateObject<RendererInstance>(
        std::move(m_shader),
        Handle<RenderPass>(m_render_pass),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
            },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .cull_faces = FaceCullMode::NONE,
                .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
            }
        )
    );
    
    InitObject(m_framebuffer);
    
    Engine::Get()->AddRendererInstance(m_renderer_instance);
    
    for (auto &item : Engine::Get()->GetDeferredSystem().Get(Bucket::BUCKET_OPAQUE).GetRendererInstances()) {
        for (auto &entity : item->GetEntities()) {
            if (entity != nullptr) {
                m_renderer_instance->AddEntity(Handle<Entity>(entity));
            }
        }
    }

    InitObject(m_renderer_instance);
}

void Voxelizer::CreateBuffers()
{
    m_counter = std::make_unique<AtomicCounter>();
    m_fragment_list_buffer = std::make_unique<StorageBuffer>();

    struct RENDER_COMMAND(CreateVoxelizerBuffers) : RenderCommand
    {
        Voxelizer &voxelizer;

        RENDER_COMMAND(CreateVoxelizerBuffers)(Voxelizer &voxelizer)
            : voxelizer(voxelizer)
        {
        }

        virtual Result operator()()
        {
            auto result = Result::OK;

            voxelizer.m_counter->Create();
            
            HYPERION_PASS_ERRORS(
                voxelizer.m_fragment_list_buffer->Create(Engine::Get()->GetGPUInstance()->GetDevice(), default_fragment_list_buffer_size),
                result
            );

            return result;
        }
    };

    RenderCommands::Push<RENDER_COMMAND(CreateVoxelizerBuffers)>(*this);
}

void Voxelizer::CreateShader()
{
    std::vector<SubShader> sub_shaders = {
        {ShaderModule::Type::VERTEX, {FileByteReader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "/vkshaders/voxel/voxelize.vert.spv")).Read()}},
        {ShaderModule::Type::FRAGMENT, {FileByteReader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "/vkshaders/voxel/voxelize.frag.spv")).Read()}}
    };

    if (Engine::Get()->GetGPUDevice()->GetFeatures().SupportsGeometryShaders()) {
        sub_shaders.push_back({ShaderModule::Type::GEOMETRY, {FileByteReader(FileSystem::Join(Engine::Get()->GetAssetManager().GetBasePath().Data(), "/vkshaders/voxel/voxelize.geom.spv")).Read()}});
    } else {
        DebugLog(
            LogType::Debug,
            "Geometry shaders not supported on device, continuing without adding geometry shader to voxelizer pipeline.\n"
        );
    }
    
    m_shader = CreateObject<Shader>(sub_shaders);
    InitObject(m_shader);
}

void Voxelizer::CreateRenderPass()
{
    m_render_pass = CreateObject<RenderPass>(
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    InitObject(m_render_pass);
}

void Voxelizer::CreateFramebuffer()
{
    m_framebuffer = CreateObject<Framebuffer>(
        Extent2D { voxel_map_size, voxel_map_size },
        Handle<RenderPass>(m_render_pass)
    );
    
    InitObject(m_framebuffer);
}

void Voxelizer::CreateDescriptors()
{
    struct RENDER_COMMAND(CreateVoxelizerDescriptors) : RenderCommand
    {
        Voxelizer &voxelizer;

        RENDER_COMMAND(CreateVoxelizerDescriptors)(Voxelizer &voxelizer)
            : voxelizer(voxelizer)
        {
        }

        virtual Result operator()()
        {
            auto *descriptor_set = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

            descriptor_set
                ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(0)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .buffer = voxelizer.m_counter->GetBuffer()
                });

            descriptor_set
                ->GetOrAddDescriptor<renderer::StorageBufferDescriptor>(1)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .buffer = voxelizer.m_fragment_list_buffer.get()
                });

            HYPERION_RETURN_OK;
        }
    };

    RenderCommands::Push<RENDER_COMMAND(CreateVoxelizerDescriptors)>(*this);
}

/* We only reconstruct the buffer if the number of rendered fragments is
 * greater than what our buffer can hold (or the buffer has not yet been created).
 * We round up to the nearest power of two. */
void Voxelizer::ResizeFragmentListBuffer(Frame *)
{
    const SizeType new_size = m_num_fragments * sizeof(Fragment);

    if (new_size <= m_fragment_list_buffer->size) {
        return;
    }

    //new_size = MathUtil::NextPowerOf2(new_size);

    DebugLog(
        LogType::Debug,
        "Resizing voxelizer fragment list buffer from %llu to %llu\n",
        m_fragment_list_buffer->size,
        new_size
    );

    Engine::Get()->SafeRelease(UniquePtr<StorageBuffer>(m_fragment_list_buffer.release()));

    m_fragment_list_buffer.reset(new StorageBuffer);
    
    HYPERION_ASSERT_RESULT(m_fragment_list_buffer->Create(
        Engine::Get()->GetGPUInstance()->GetDevice(),
        new_size
    ));

    // TODO! frame index
    auto *descriptor_set = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

    descriptor_set->GetDescriptor(1)->RemoveSubDescriptor(0);
    descriptor_set->GetDescriptor(1)->SetSubDescriptor({
        .element_index = 0u,
        .buffer = m_fragment_list_buffer.get()
    });

    descriptor_set->ApplyUpdates(Engine::Get()->GetGPUInstance()->GetDevice());
}

void Voxelizer::RenderFragmentList(Frame *, bool count_mode)
{
    m_renderer_instance->GetPipeline()->push_constants.voxelizer_data = {
        .grid_size = voxel_map_size,
        .count_mode = count_mode
    };

    auto single_time_commands = Engine::Get()->GetGPUInstance()->GetSingleTimeCommands();

    single_time_commands.Push([&](CommandBuffer *command_buffer) {
        auto temp_frame = Frame::TemporaryFrame(command_buffer);

        m_framebuffer->BeginCapture(command_buffer);
        
        if (!Engine::Get()->render_state.GetScene()) {
            Engine::Get()->render_state.BindScene(m_scene.Get());
            m_renderer_instance->Render(&temp_frame);
            Engine::Get()->render_state.UnbindScene();
        } else {
            m_renderer_instance->Render(&temp_frame);
        }

        m_framebuffer->EndCapture(command_buffer);

        HYPERION_RETURN_OK;
    });

    HYPERION_ASSERT_RESULT(single_time_commands.Execute(Engine::Get()->GetGPUDevice()));
}

void Voxelizer::Render(Frame *frame)
{
    m_scene->GetCamera()->UpdateMatrices();

    m_counter->Reset();

    RenderFragmentList(frame, true);

    m_num_fragments = m_counter->Read();
    DebugLog(LogType::Debug, "Render %lu fragments (%llu MiB)\n", m_num_fragments, (m_num_fragments * sizeof(Fragment)) / 1000000ull);

    ResizeFragmentListBuffer(frame);

    m_counter->Reset();

    /* now we render the scene again, this time storing color values into the
     * fragment list buffer.  */
    RenderFragmentList(frame, false);
}

} // namespace hyperion::v2
