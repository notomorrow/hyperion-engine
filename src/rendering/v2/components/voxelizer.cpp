#include "voxelizer.h"
#include "../engine.h"

#include <math/math_util.h>
#include <asset/byte_reader.h>
#include <asset/asset_manager.h> /* TMP */
#include <rendering/camera/ortho_camera.h>

namespace hyperion::v2 {

Voxelizer::Voxelizer()
    : EngineComponentBase(),
      m_num_fragments(0)
{
}

Voxelizer::~Voxelizer()
{
    Teardown();
}

void Voxelizer::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_VOXELIZER, [this](Engine *engine) {
        m_scene = engine->resources.scenes.Add(std::make_unique<Scene>(
            std::make_unique<OrthoCamera>(
                -voxel_map_size, voxel_map_size,
                -voxel_map_size, voxel_map_size,
                -voxel_map_size, voxel_map_size
            )
        ));

        if (m_counter == nullptr) {
            m_counter = std::make_unique<AtomicCounter>();
            m_counter->Create(engine);
        }

        if (m_fragment_list_buffer == nullptr) {
            m_fragment_list_buffer = std::make_unique<StorageBuffer>();

            HYPERION_ASSERT_RESULT(m_fragment_list_buffer->Create(engine->GetInstance()->GetDevice(), default_fragment_list_buffer_size));
        }
        
        CreateShader(engine);
        CreateRenderPass(engine);
        CreateFramebuffer(engine);
        CreateDescriptors(engine);

        /* TODO: once descriptors are the new component type, this should be removed. */
        //engine->callbacks.Once(EngineCallback::CREATE_DESCRIPTOR_SETS, [this](Engine *engine) {
        //});
        
        CreatePipeline(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_VOXELIZER, [this](Engine *engine) {
            auto result = renderer::Result::OK;

            if (m_counter != nullptr) {
                m_counter->Destroy(engine);
                m_counter.reset();
            }

            if (m_fragment_list_buffer != nullptr) {
                HYPERION_PASS_ERRORS(
                    m_fragment_list_buffer->Destroy(engine->GetInstance()->GetDevice()),
                    result
                );

                m_fragment_list_buffer.reset();
            }

            m_shader      = nullptr;
            m_framebuffer = nullptr;
            m_render_pass = nullptr;

            for (auto &attachment : m_attachments) {
                HYPERION_PASS_ERRORS(
                    attachment->Destroy(engine->GetInstance()->GetDevice()),
                    result
                );
            }

            m_attachments.clear();

            m_pipeline = nullptr;

            m_num_fragments = 0;

            HYPERION_ASSERT_RESULT(result);
        }), engine);
    }));
}

void Voxelizer::CreatePipeline(Engine *engine)
{
    auto pipeline = std::make_unique<GraphicsPipeline>(
        std::move(m_shader),
        m_scene.Acquire(),
        m_render_pass.Acquire(),
        GraphicsPipeline::Bucket::BUCKET_VOXELIZER
    );

    pipeline->SetDepthWrite(false);
    pipeline->SetDepthTest(false);
    pipeline->SetCullMode(CullMode::NONE);
    
    pipeline->AddFramebuffer(m_framebuffer.Acquire());
    
    m_pipeline = engine->AddGraphicsPipeline(std::move(pipeline));
    
    for (auto &pipeline : engine->GetRenderList().Get(GraphicsPipeline::BUCKET_OPAQUE).m_graphics_pipelines) {
        for (auto &spatial : pipeline->GetSpatials()) {
            if (spatial != nullptr) {
                m_pipeline->AddSpatial(spatial.Acquire());
            }
        }
    }

    m_pipeline.Init();
}

void Voxelizer::CreateShader(Engine *engine)
{
    m_shader = engine->resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            {ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/voxel/voxelize.vert.spv").Read()}},
            {ShaderModule::Type::GEOMETRY, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/voxel/voxelize.geom.spv").Read()}},
            {ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/voxel/voxelize.frag.spv").Read()}}
        }
    ));

    m_shader->Init(engine);
}

void Voxelizer::CreateRenderPass(Engine *engine)
{
    m_render_pass = engine->resources.render_passes.Add(std::make_unique<RenderPass>(
        RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    ));

    m_render_pass.Init();
}

void Voxelizer::CreateFramebuffer(Engine *engine)
{
    m_framebuffer = engine->resources.framebuffers.Add(std::make_unique<Framebuffer>(
        Extent2D{voxel_map_size, voxel_map_size},
        m_render_pass.Acquire()
    ));
    
    m_framebuffer.Init();
}

void Voxelizer::CreateDescriptors(Engine *engine)
{
    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

    descriptor_set
        ->AddDescriptor<renderer::StorageBufferDescriptor>(0)
        ->AddSubDescriptor({.gpu_buffer = m_counter->GetBuffer()});

    descriptor_set
        ->AddDescriptor<renderer::StorageBufferDescriptor>(1)
        ->AddSubDescriptor({.gpu_buffer = m_fragment_list_buffer.get()});
}

/* We only reconstruct the buffer if the number of rendered fragments is
 * greater than what our buffer can hold (or the buffer has not yet been created).
 * We round up to the nearest power of two. */
void Voxelizer::ResizeFragmentListBuffer(Engine *engine)
{
    const size_t new_size = m_num_fragments * sizeof(Fragment);

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

    HYPERION_ASSERT_RESULT(m_fragment_list_buffer->Destroy(
        engine->GetInstance()->GetDevice()
    ));

    m_fragment_list_buffer.reset(new StorageBuffer);
    
    HYPERION_ASSERT_RESULT(m_fragment_list_buffer->Create(
        engine->GetInstance()->GetDevice(),
        new_size
    ));

    auto *descriptor_set = engine->GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER);

    descriptor_set->GetDescriptor(1)->RemoveSubDescriptor(0);
    descriptor_set->GetDescriptor(1)->AddSubDescriptor({.gpu_buffer = m_fragment_list_buffer.get()});

    descriptor_set->ApplyUpdates(engine->GetInstance()->GetDevice());
}

void Voxelizer::RenderFragmentList(Engine *engine, bool count_mode)
{
    auto commands = engine->GetInstance()->GetSingleTimeCommands();

    /* count number of fragments */
    commands.Push([this, engine, count_mode](CommandBuffer *command_buffer) {
        m_pipeline->Get().push_constants.voxelizer_data = {
            .grid_size = voxel_map_size,
            .count_mode = count_mode
        };

        m_framebuffer->BeginCapture(command_buffer);
        m_pipeline->Render(engine, command_buffer, 0);
        m_framebuffer->EndCapture(command_buffer);

        HYPERION_RETURN_OK;
    });

    HYPERION_ASSERT_RESULT(commands.Execute(engine->GetInstance()->GetDevice()));
}

void Voxelizer::Render(Engine *engine)
{
    m_scene->GetCamera()->UpdateMatrices();

    m_counter->Reset(engine);

    RenderFragmentList(engine, true);

    m_num_fragments = m_counter->Read(engine);
    DebugLog(LogType::Debug, "Render %lu fragments (%llu MiB)\n", m_num_fragments, (m_num_fragments * sizeof(Fragment)) / 1000000ull);

    ResizeFragmentListBuffer(engine);

    m_counter->Reset(engine);

    /* now we render the scene again, this time storing color values into the
     * fragment list buffer.  */
    RenderFragmentList(engine, false);
}

} // namespace hyperion::v2