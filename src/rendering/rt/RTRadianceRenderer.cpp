#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Extent3D;
using renderer::ImageDescriptor;
using renderer::StorageImageDescriptor;
using renderer::StorageBufferDescriptor;
using renderer::TlasDescriptor;
using renderer::ResourceState;
using renderer::Result;

struct RENDER_COMMAND(CreateRTRadianceDescriptorSets) : RenderCommandBase2
{
    UniquePtr<DescriptorSet> *descriptor_sets;
    FixedArray<renderer::ImageView *, max_frames_in_flight> image_views;

    RENDER_COMMAND(CreateRTRadianceDescriptorSets)(UniquePtr<DescriptorSet> *descriptor_sets, FixedArray<renderer::ImageView *, max_frames_in_flight> &&image_views)
        : descriptor_sets(descriptor_sets),
          image_views(std::move(image_views))
    {
    }

    virtual Result operator()(Engine *engine)
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            // create our own descriptor sets
            AssertThrow(descriptor_sets[frame_index] != nullptr);
            
            HYPERION_BUBBLE_ERRORS(descriptor_sets[frame_index]->Create(
                engine->GetDevice(),
                &engine->GetInstance()->GetDescriptorPool()
            ));

            // Add the final result to the global descriptor set
            auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_RADIANCE_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = image_views[frame_index]
                });
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateRTRadiancePipeline) : RenderCommandBase2
{
    renderer::RaytracingPipeline *pipeline;
    renderer::ShaderProgram *shader_program;

    RENDER_COMMAND(CreateRTRadiancePipeline)(renderer::RaytracingPipeline *pipeline, renderer::ShaderProgram *shader_program)
        : pipeline(pipeline),
          shader_program(shader_program)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        return pipeline->Create(
            engine->GetDevice(),
            shader_program,
            &engine->GetInstance()->GetDescriptorPool()
        );
    }
};

struct RENDER_COMMAND(DestroyRTRadianceRenderer) : RenderCommandBase2
{
    RTRadianceRenderer::ImageOutput *image_outputs;

    RENDER_COMMAND(DestroyRTRadianceRenderer)(RTRadianceRenderer::ImageOutput *image_outputs)
        : image_outputs(image_outputs)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        auto result = Result::OK;

        // remove result image from global descriptor set
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_PASS_ERRORS(
                image_outputs[frame_index].Destroy(engine->GetDevice()),
                result
            );

            auto *descriptor_set_globals = engine->GetInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::global_buffer_mapping[frame_index]);

            // set to placeholder image
            descriptor_set_globals
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::RT_RADIANCE_RESULT)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .image_view = &engine->GetPlaceholderData().GetImageView2D1x1R8()
                });
        }

        return result;
    }
};

struct RENDER_COMMAND(CreateRTRadianceImageOutputs) : RenderCommandBase2
{
    RTRadianceRenderer::ImageOutput *image_outputs;

    RENDER_COMMAND(CreateRTRadianceImageOutputs)(RTRadianceRenderer::ImageOutput *image_outputs)
        : image_outputs(image_outputs)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(image_outputs[frame_index].Create(engine->GetDevice()));
        }

        HYPERION_RETURN_OK;
    }
};

Result RTRadianceRenderer::ImageOutput::Create(Device *device)
{
    HYPERION_BUBBLE_ERRORS(image.Create(device));
    HYPERION_BUBBLE_ERRORS(image_view.Create(device, &image));

    HYPERION_RETURN_OK;
}

Result RTRadianceRenderer::ImageOutput::Destroy(Device *device)
{
    auto result = Result::OK;

    HYPERION_PASS_ERRORS(
        image.Destroy(device),
        result
    );

    HYPERION_PASS_ERRORS(
        image_view.Destroy(device),
        result
    );

    return result;
}

RTRadianceRenderer::RTRadianceRenderer(const Extent2D &extent)
    : m_extent(extent),
      m_image_outputs {
          ImageOutput(StorageImage(
              Extent3D(extent),
              InternalFormat::RGBA8,
              ImageType::TEXTURE_TYPE_2D
          )),
          ImageOutput(StorageImage(
              Extent3D(extent),
              InternalFormat::RGBA8,
              ImageType::TEXTURE_TYPE_2D
          ))
      },
      m_temporal_blending(
          extent,
          FixedArray<Image *, max_frames_in_flight> { &m_image_outputs[0].image, &m_image_outputs[1].image },
          FixedArray<ImageView *, max_frames_in_flight> { &m_image_outputs[0].image_view, &m_image_outputs[1].image_view }
      ),
      m_has_tlas_updates { false, false }
{
}

RTRadianceRenderer::~RTRadianceRenderer()
{
}

void RTRadianceRenderer::Create(Engine *engine)
{
    AssertThrowMsg(
        engine->InitObject(m_tlas),
        "Failed to initialize the top level acceleration structure!"
    );

    CreateImages(engine);
    CreateTemporalBlending(engine);
    CreateDescriptorSets(engine);
    CreateRaytracingPipeline(engine);
}

void RTRadianceRenderer::Destroy(Engine *engine)
{
    m_temporal_blending.Destroy(engine);

    engine->SafeReleaseHandle(std::move(m_shader));

    engine->SafeRelease(std::move(m_raytracing_pipeline));

    // release our owned descriptor sets
    for (auto &descriptor_set : m_descriptor_sets) {
        engine->SafeRelease(std::move(descriptor_set));
    }

    RenderCommands::Push<RENDER_COMMAND(DestroyRTRadianceRenderer)>(m_image_outputs.Data());

    HYP_FLUSH_RENDER_QUEUE(engine);
}

void RTRadianceRenderer::Render(
    Engine *engine,
    Frame *frame
)
{
    if (m_has_tlas_updates[frame->GetFrameIndex()]) {
        m_descriptor_sets[frame->GetFrameIndex()]->ApplyUpdates(engine->GetDevice());

        m_has_tlas_updates[frame->GetFrameIndex()] = false;
    }

    m_raytracing_pipeline->Bind(frame->GetCommandBuffer());

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_raytracing_pipeline.Get(),
        m_descriptor_sets[frame->GetFrameIndex()].Get(),
        0
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_raytracing_pipeline.Get(),
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE, frame->GetFrameIndex()),
        1,
        FixedArray {
            UInt32(sizeof(SceneShaderData) * engine->render_state.GetScene().id.ToIndex()),
            UInt32(sizeof(LightDrawProxy) * 0)
        }
    );

    frame->GetCommandBuffer()->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_raytracing_pipeline.Get(),
        DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS, frame->GetFrameIndex()),
        2
    );

    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::UNORDERED_ACCESS
    );

    m_raytracing_pipeline->TraceRays(
        engine->GetDevice(),
        frame->GetCommandBuffer(),
        m_image_outputs[frame->GetFrameIndex()].image.GetExtent()
    );

    m_image_outputs[frame->GetFrameIndex()].image.GetGPUImage()->InsertBarrier(
        frame->GetCommandBuffer(),
        ResourceState::SHADER_RESOURCE
    );

    m_temporal_blending.Render(engine, frame);
}

void RTRadianceRenderer::CreateImages(Engine *engine)
{
    RenderCommands::Push<RENDER_COMMAND(CreateRTRadianceImageOutputs)>(m_image_outputs.Data());
}

void RTRadianceRenderer::ApplyTLASUpdates(Engine *engine, RTUpdateStateFlags flags)
{
    if (!flags) {
        return;
    }
    
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE) {
            // update acceleration structure in descriptor set
            m_descriptor_sets[frame_index]->GetDescriptor(0)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .acceleration_structure = &m_tlas->GetInternalTLAS()
                });
        }

        if (flags & renderer::RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS) {
            // update mesh descriptions buffer in descriptor set
            m_descriptor_sets[frame_index]->GetDescriptor(2)
                ->SetSubDescriptor({
                    .element_index = 0u,
                    .buffer = m_tlas->GetInternalTLAS().GetMeshDescriptionsBuffer()
                });
        }

        m_has_tlas_updates[frame_index] = true;
    }
}

void RTRadianceRenderer::CreateDescriptorSets(Engine *engine)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        auto descriptor_set = UniquePtr<DescriptorSet>::Construct();

        descriptor_set->GetOrAddDescriptor<TlasDescriptor>(0)
            ->SetSubDescriptor({
                .element_index = 0u,
                .acceleration_structure = &m_tlas->GetInternalTLAS()
            });

        descriptor_set->GetOrAddDescriptor<StorageImageDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = &m_image_outputs[frame_index].image_view
            });
        
        // mesh descriptions
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = m_tlas->GetInternalTLAS().GetMeshDescriptionsBuffer()
            });
        
        // materials
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(3)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = engine->shader_globals->materials.GetBuffers()[frame_index].get()
            });
        
        // entities
        descriptor_set->GetOrAddDescriptor<StorageBufferDescriptor>(4)
            ->SetSubDescriptor({
                .element_index = 0u,
                .buffer = engine->shader_globals->objects.GetBuffers()[frame_index].get()
            });

        m_descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    RenderCommands::Push<RENDER_COMMAND(CreateRTRadianceDescriptorSets)>(
        m_descriptor_sets.Data(),
        FixedArray<ImageView *, max_frames_in_flight> {
            &m_temporal_blending.GetImageOutput(0).image_view,
            &m_temporal_blending.GetImageOutput(1).image_view
        }
    );
}

void RTRadianceRenderer::CreateRaytracingPipeline(Engine *engine)
{
    m_shader = engine->CreateHandle<Shader>(engine->GetShaderCompiler().GetCompiledShader("RTRadiance"));
    if (!engine->InitObject(m_shader)) {
        return;
    }

    m_raytracing_pipeline.Reset(new RaytracingPipeline(
        Array<const DescriptorSet *> {
            m_descriptor_sets[0].Get(),
            engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE),
            engine->GetInstance()->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS)
        }
    ));

    engine->callbacks.Once(EngineCallback::CREATE_RAYTRACING_PIPELINES, [this, engine](...) {
        RenderCommands::Push<RENDER_COMMAND(CreateRTRadiancePipeline)>(
            m_raytracing_pipeline.Get(),
            m_shader->GetShaderProgram()
        );
    });
}

void RTRadianceRenderer::CreateTemporalBlending(Engine *engine)
{
    m_temporal_blending.Create(engine);
}

} // namespace hyperion::v2